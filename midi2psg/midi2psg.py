#!/usr/bin/env python3
"""
MIDI to 60 FPS frequency/volume CSV converter.

Extracts pitch and velocity data from MIDI files and outputs
at 60 frames per second resolution with tick counts and 0-255 volume.
Each channel outputs to a separate file.
"""

# TODO: Add command-line option to set polyphony (right now it is hard coded to 3 channels per note)
# TODO: add command-line option to arpeggio (2) polyphony channels for chords

import mido
import math
import sys
import os
from typing import List, Tuple, Dict, Set
from dataclasses import dataclass

# Configuration for volume decay per frame
VOLUME_DECAY_PER_FRAME = 2.5  # Configurable decay amount

@dataclass
class Note:
    """Represents a musical note with timing and pitch information."""
    start_time: float  # in seconds
    duration: float    # in seconds
    frequency: float   # in Hz
    velocity: int      # MIDI velocity (0-127)
    channel: int       # MIDI channel

@dataclass
class ActiveNote:
    """Represents an active note with decay tracking."""
    note: Note
    current_volume: float  # Current volume level (0-255) - float for gradual decay
    frames_active: int     # Number of frames this note has been active

def midi_to_frequency(midi_note: int) -> float:
    """
    Convert MIDI note number to frequency in Hz.
    A4 (440 Hz) is MIDI note 69.
    """
    return 440.0 * (2.0 ** ((midi_note - 69) / 12.0))

def frequency_to_tick_count(frequency: float) -> int:
    """
    Convert frequency in Hz to tick counts using formula: 111860.9/Hz
    
    Args:
        frequency: Frequency in Hz
        
    Returns:
        Tick count value as integer
    """
    if frequency <= 0:
        return 0
    return int(round(111860.9 / frequency))

def velocity_to_255_range(velocity: int) -> float:
    """
    Convert MIDI velocity (0-127) to 0-255 range.
    0 = mute, 255 = maximum volume
    
    Args:
        velocity: MIDI velocity value
        
    Returns:
        Volume in 0-255 range as float
    """
    return (velocity / 127.0) * 255.0

def is_drum_channel(channel: int) -> bool:
    """
    Determine if a MIDI channel is typically used for drums.
    Channel 9 (0-indexed) is the standard drum channel in MIDI.
    """
    return channel == 9

def get_channel_type(channel: int) -> str:
    """
    Get the channel type identifier for filename.
    """
    return "noi" if is_drum_channel(channel) else "ton"

def parse_midi(file_path: str, speed_scale: float = 1.0) -> Tuple[Dict[int, List[Note]], float, Set[int]]:
    """
    Parse MIDI file and extract note information by track (treating tracks as channels).
    
    Args:
        file_path: Path to MIDI file
        speed_scale: Speed multiplier (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
        
    Returns:
        Tuple of (notes_by_channel, total_duration_seconds, active_channels)
    """
    mid = mido.MidiFile(file_path)
    
    print(f"MIDI file info: {len(mid.tracks)} tracks, ticks_per_beat: {mid.ticks_per_beat}")
    
    # Track active notes per track (treating tracks as channels)
    notes_by_channel = {}  # track_id -> List[Note]
    active_channels = set()
    
    # Track tempo changes globally
    current_tempo = 500000  # Default tempo (120 BPM)
    
    # Process all tracks
    for track_idx, track in enumerate(mid.tracks):
        print(f"Processing track {track_idx}: {len(track)} messages")
        track_time = 0.0
        track_active_notes = {}  # pitch -> (start_time, velocity)
        track_has_notes = False
        
        for msg in track:
            # Convert ticks to seconds using current tempo and apply speed scaling
            track_time += mido.tick2second(msg.time, mid.ticks_per_beat, current_tempo) / speed_scale
            
            if msg.type == 'set_tempo':
                # Update current tempo
                current_tempo = msg.tempo
                continue
            
            elif msg.type == 'note_on' and msg.velocity > 0:
                # Note starts
                track_has_notes = True
                track_active_notes[msg.note] = (track_time, msg.velocity)
            
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                # Note ends
                if msg.note in track_active_notes:
                    start_time, velocity = track_active_notes[msg.note]
                    duration = track_time - start_time
                    
                    # Convert MIDI note to frequency
                    frequency = midi_to_frequency(msg.note)
                    
                    # Create note object (using track_idx as channel)
                    note = Note(
                        start_time=start_time,
                        duration=duration,
                        frequency=frequency,
                        velocity=velocity,
                        channel=track_idx  # Use track index as channel
                    )
                    
                    # Add to track's note list
                    if track_idx not in notes_by_channel:
                        notes_by_channel[track_idx] = []
                    notes_by_channel[track_idx].append(note)
                    
                    # Remove from active notes
                    del track_active_notes[msg.note]
        
        if track_has_notes:
            active_channels.add(track_idx)
            print(f"  Track {track_idx}: {len(notes_by_channel.get(track_idx, []))} notes")
        else:
            print(f"  Track {track_idx}: no notes (likely tempo/control track)")
        
        # Handle any remaining active notes in this track
        all_notes = []
        for channel_notes in notes_by_channel.values():
            all_notes.extend(channel_notes)
        max_time = max((note.start_time + note.duration for note in all_notes), default=track_time)
        
        for pitch, (start_time, velocity) in track_active_notes.items():
            duration = max_time - start_time
            frequency = midi_to_frequency(pitch)
            
            note = Note(
                start_time=start_time,
                duration=duration,
                frequency=frequency,
                velocity=velocity,
                channel=track_idx
            )
            
            if track_idx not in notes_by_channel:
                notes_by_channel[track_idx] = []
            notes_by_channel[track_idx].append(note)
    
    # Sort notes by start time for each track
    for track_idx in notes_by_channel:
        notes_by_channel[track_idx].sort(key=lambda n: n.start_time)
    
    # Calculate total duration
    all_notes = []
    for channel_notes in notes_by_channel.values():
        all_notes.extend(channel_notes)
    total_duration = max((note.start_time + note.duration for note in all_notes), default=0.0)
    
    return notes_by_channel, total_duration, active_channels

def generate_channel_outputs(notes: List[Note], total_duration: float, base_filename: str, track_idx: int, output_channel: int, file_index: int, decay_scale: float = 1.0):
    """
    Generate 60 FPS CSV output for a single track, split into 3 separate note files.
    
    Args:
        notes: List of Note objects for this track
        total_duration: Total song duration in seconds
        base_filename: Base filename without extension
        track_idx: Original track index
        output_channel: Renumbered output channel (starting from 0)
        file_index: Sequential file index for naming (passed by reference)
        decay_scale: Decay rate multiplier (1.0 = normal, 0.5 = slower decay, 2.0 = faster decay)
        
    Returns:
        Updated file_index value
    """
    fps = 60
    frame_duration = 1.0 / fps
    total_frames = int(math.ceil(total_duration * fps))
    
    # Track active notes across frames for volume decay
    active_notes_tracker = []  # List of ActiveNote objects
    
    # Prepare data for all 3 note slots
    note_data = [[] for _ in range(3)]  # 3 lists for 3 note slots
    
    for frame in range(total_frames):
        frame_time = frame * frame_duration
        
        # Update existing active notes - apply decay and remove expired notes
        updated_active_notes = []
        for active_note in active_notes_tracker:
            note_end_time = active_note.note.start_time + active_note.note.duration
            
            # Check if note is still active
            if frame_time < note_end_time:
                # Apply volume decay - decrease volume over time with scaling
                active_note.frames_active += 1
                decay_amount = active_note.frames_active * VOLUME_DECAY_PER_FRAME * decay_scale
                active_note.current_volume = max(0.0, velocity_to_255_range(active_note.note.velocity) - decay_amount)
                
                # Only keep if not completely muted
                if active_note.current_volume > 0.0:
                    updated_active_notes.append(active_note)
        
        active_notes_tracker = updated_active_notes
        
        # Add newly started notes
        for note in notes:
            # Check if note starts at this frame (within frame tolerance)
            if (note.start_time <= frame_time < note.start_time + frame_duration and
                frame_time < note.start_time + note.duration):
                
                # Check if this note is already being tracked
                already_tracked = False
                for active_note in active_notes_tracker:
                    if (active_note.note.frequency == note.frequency and 
                        abs(active_note.note.start_time - note.start_time) < frame_duration):
                        already_tracked = True
                        break
                
                if not already_tracked:
                    active_note = ActiveNote(
                        note=note,
                        current_volume=velocity_to_255_range(note.velocity),
                        frames_active=0
                    )
                    active_notes_tracker.append(active_note)
        
        # Get top 3 notes by frequency for this frame
        if active_notes_tracker:
            # Sort by frequency (highest first) and take top 3
            active_notes_tracker.sort(key=lambda an: an.note.frequency, reverse=True)
            top_notes = active_notes_tracker[:3]
        else:
            top_notes = []
        
        # Store data for each of the 3 note slots
        for slot in range(3):
            if slot < len(top_notes):
                active_note = top_notes[slot]
                tick_count = frequency_to_tick_count(active_note.note.frequency)
                volume_int = int(active_note.current_volume)
                note_data[slot].append((tick_count, volume_int))
            else:
                # No note for this slot
                note_data[slot].append((0, 0))
    
    # Write files for each note slot that has non-zero data
    channel_type = get_channel_type(output_channel)
    files_created = 0
    
    for slot in range(3):
        # Check if this slot has any non-zero data
        has_data = any(tick != 0 or vol != 0 for tick, vol in note_data[slot])
        
        if has_data:
            # Create filename with sequential index
            note_filename = f"{base_filename}_{channel_type}{file_index:02d}.60hz"
            
            with open(note_filename, 'w') as f:
                for tick_count, volume_int in note_data[slot]:
                    f.write(f"{tick_count},{volume_int}\n")
            
            print(f"  Note slot {slot}: {note_filename}")
            files_created += 1
            file_index += 1
    
    return file_index

def main():
    """Main function to process command line arguments and run conversion."""
    if len(sys.argv) < 2 or len(sys.argv) > 4:
        print("Usage: python midi_converter.py <input.mid> [speed_scale] [decay_scale]")
        print("  speed_scale: Song speed multiplier (default 1.0, 0.5=half speed, 2.0=double speed)")
        print("  decay_scale: Volume decay rate multiplier (default 1.0, 0.5=slower decay, 2.0=faster decay)")
        print(f"Current default volume decay per frame: {VOLUME_DECAY_PER_FRAME}")
        print("Note: Requires 'mido' library - install with: pip install mido")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    # Parse optional scaling parameters
    speed_scale = 1.0
    decay_scale = 1.0
    
    if len(sys.argv) >= 3:
        try:
            speed_scale = float(sys.argv[2])
            if speed_scale <= 0:
                print("Error: speed_scale must be positive")
                sys.exit(1)
        except ValueError:
            print("Error: speed_scale must be a valid number")
            sys.exit(1)
    
    if len(sys.argv) >= 4:
        try:
            decay_scale = float(sys.argv[3])
            if decay_scale < 0:
                print("Error: decay_scale must be non-negative")
                sys.exit(1)
        except ValueError:
            print("Error: decay_scale must be a valid number")
            sys.exit(1)
    
    try:
        print(f"Parsing MIDI file: {input_file}")
        print(f"Speed scale: {speed_scale}x")
        print(f"Decay scale: {decay_scale}x")
        notes_by_channel, duration, active_channels = parse_midi(input_file, speed_scale)
        
        total_notes = sum(len(notes) for notes in notes_by_channel.values())
        print(f"Found {total_notes} notes across {len(active_channels)} channels")
        print(f"Total duration: {duration:.2f} seconds")
        print(f"Active channels: {sorted(active_channels)}")
        print(f"Effective volume decay per frame: {VOLUME_DECAY_PER_FRAME * decay_scale}")
        
        # Generate output for each active channel with sequential file indexing
        output_channel = 0
        file_index = 0  # Sequential index for all files
        total_files_created = 0
        
        for channel in sorted(active_channels):
            channel_notes = notes_by_channel.get(channel, [])
            #base_filename = os.path.splitext(input_file)[0]
            base_filename = input_file   # don't remove extension in this case
            
            print(f"Generating track {channel} -> channel {output_channel} ({get_channel_type(output_channel)}):")
            print(f"  {len(channel_notes)} notes")
            
            files_created_before = file_index
            file_index = generate_channel_outputs(channel_notes, duration, base_filename, channel, output_channel, file_index, decay_scale)
            files_created = file_index - files_created_before
            total_files_created += files_created
            output_channel += 1
        
        print(f"Total files created: {total_files_created}")
        
        print("Conversion complete.")
        
    except ImportError:
        print("Error: 'mido' library not found. Install with: pip install mido")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()