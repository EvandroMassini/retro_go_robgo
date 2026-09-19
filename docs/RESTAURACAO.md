# Restauração da base anterior às experiências de áudio

**Registro histórico. A entrega atual é [robgo-audio-quality2](AUDIO-QUALITY2.md),
com imagem completa e aplicação isolada geradas juntas. Os tamanhos e hashes
abaixo identificam a cópia restaurada naquela ocasião.**

Após o relato de reinícios em Space Manbow nas revisões `robgo-audio-fidelity1` e `robgo-f10-scc-fix2`, foi reposto o aplicativo isolado salvo antes dessas alterações. A causa do reinício não foi comprovada; esta é uma reversão, não uma correção diagnosticada.

- Arquivo: `dist/firmware.bin`, **584.208 bytes**, sem merge.
- Versão exibida pelo Retro-Go: `1.46-8-g4ced1-dirty`.
- Versão no descritor ESP-IDF desse binário histórico: `1.46-8-g4ced1206-dirty`.
- SHA-256: `cba3f9de77a5fef7bb83ad005420ffe4a7a9325852ae0112238dbda460d0f5ce`.

O arquivo foi restaurado byte a byte, sem recompilação. Os seis arquivos de execução alterados nas experiências também foram restaurados a partir do snapshot anterior: frontend main, Sound, SCC, Console, MSX e msxfix.h. A versão experimental e seus símbolos foram arquivados fora da pasta de distribuição. Não há `firmware.elf` correspondente a esta cópia histórica em dist; outros ELFs/manifestos da pasta pertencem às respectivas compilações completas e não devem ser usados como símbolos deste aplicativo.

Foram revertidos o ganho de +3,52 dB, a curva de compressão, as mudanças recentes de síntese/reset SCC/tons, a ordenação alfabética nova e o registro independente de LastGame pelo F10. Permanecem as adaptações anteriores: GPIOs, bootl.rc, logo, menus azuis, navegação `..`, proporções de monitor e saída de áudio anteriormente utilizada.

As limitações sonoras conhecidas da base anterior voltam a existir. A ordem do F10 volta a ser a fornecida pelo FAT; a restauração do último jogo continua dependendo do mecanismo anterior/launcher. O registro experimental `/retro-go/config/robgo-msx.json`, se presente no cartão, não é usado nesta base e não foi apagado.

Checksum/hash interno e igualdade com o backup foram verificados. Teste físico de Space Manbow ainda depende do mantenedor. Instale o aplicativo com o bootloader já utilizado; não se presume seu mapa de partições.
