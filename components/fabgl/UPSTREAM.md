FabGL v1.0.9, commit 4edd0d5fcb83b7e454c78ccb69ada1d18582f1e9
https://github.com/fdivitto/FabGL
Licença preservada. Alteração local: inclusão de <cstring> em src/fabutils.h para compilar sem Arduino.
Os subdiretórios de headers são privados no CMake para evitar colisão entre o Z80.h da FabGL e o do fMSX.

Foi adicionado VGA16Controller::setRawPixel(x, y, index) para a implementação
anterior da ponte RobGo. A versão experimental atual usa VGAController nativo
de 64 cores. Os avisos de autoria e licença originais são preservados.
