@rem this reparses the GCC asm files for use with standard TI assembly
@rem You should not need to run this, they are shipped pre-parsed, but it's offered
@rem in case Tursi forgets.

reparseTIASM ..\libtivgm2\CPlayerCommonHandEdit.asm ..\libtivgm2\CPlayerTIHandEdit.asm ..\libtivgm2\CPlayerTIHandlers.asm tiSNonly.asm

reparseTIASM ..\libtivgm2\CPlayerCommonHandEdit.asm ..\libtivgm2\CPlayerTIHandEdit.asm ..\libtivgm2\CPlayerTIHandlers.asm ..\libtivgm2\CPlayerTISfx.asm ..\libtivgm2\CPlayerTISfxHandlers.asm tiSfxSN.asm

reparseTIASM ..\libtivgm2\CPlayerCommonHandEdit.asm ..\libtivgm2\CSIDPlayTIHandEdit.asm ..\libtivgm2\CSIDPlayTIHandlers.asm tiSIDonly.asm

reparseTIASM ..\libtivgm2\CPlayerCommonHandEdit.asm ..\libtivgm2\CPlayerTI30Hz.asm ..\libtivgm2\CPlayerTIHandlers.asm tiSNonly30.asm

reparseTIASM ..\libtivgm2\CPlayerCommonHandEdit.asm ..\libtivgm2\CPlayerTI30Hz.asm ..\libtivgm2\CPlayerTIHandlers.asm ..\libtivgm2\CPlayerTISfx30Hz.asm ..\libtivgm2\CPlayerTISfxHandlers.asm tiSfxSN30.asm

reparseTIASM ..\libtivgm2\CPlayerCommonHandEdit.asm ..\libtivgm2\CSIDPlayTI30Hz.asm ..\libtivgm2\CSIDPlayTIHandlers.asm tiSIDonly30.asm


