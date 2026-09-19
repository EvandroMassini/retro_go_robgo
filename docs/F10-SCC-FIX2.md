# Revisão robgo-f10-scc-fix2

**Revisão histórica, retirada porque Space Manbow continuou reiniciando a placa. Não descreve o firmware.bin atual. Veja [restauração](RESTAURACAO.md).**

Arquivo: `dist/firmware.bin`, aplicativo fMSX isolado, sem merge. O usuário utiliza um bootloader próprio para instalar esse aplicativo; não se pressupõe o mapa de partições desse bootloader.

## Alterações

- F10 → Open file: ordenação alfabética sem distinguir maiúsculas/minúsculas, pastas antes dos arquivos e `..` no início. A ordem anterior vinha de `readdir`/FAT; não era evidência suficiente de defeito do cartão.
- Jogos abertos pelo F10: após aceitar o cartucho/disco, o frontend registra o caminho absoluto em `/retro-go/config/robgo-msx.json`, chave `LastGame`. A gravação ocorre somente quando o caminho muda.
- Ao iniciar sem caminho fornecido pelo launcher, o aplicativo tenta esse último arquivo, desde que ainda exista. Um caminho explícito do launcher tem prioridade. Na primeira instalação desta revisão, selecione um jogo pelo F10 para criar o registro. A restauração abre o jogo, não um save state automático.
- SCC: corrigido o reset que repetia o primeiro canal em vez de configurar os cinco; canais são silenciados no reset para evitar estado antigo.
- SCC: substituída a média por somas prefixadas da revisão fidelity1 por interpolação linear com índice circular limitado a 0..31. Eliminados o vetor temporário e a divisão por amostra. Notas acima de 1 kHz continuam habilitadas; ganho e correção de tons da revisão anterior são mantidos.

## Validação e limite

Build ESP-IDF 4.4.8 concluído. Versão incorporada, igualdade com `fmsx.bin`, limite de partição, checksum e hash interno da imagem verificados. O binário tem 585.360 bytes; SHA-256 está em `dist/firmware.json`. O ELF correspondente está em `dist/firmware.elf`.

Sem teste físico nesta etapa. O erro de reset dos canais é real, mas não foi possível comprovar que era a causa do reinício em Space Manbow. Esta versão precisa ser testada nesse jogo antes de considerar o problema resolvido. A emulação FM continua simplificada.

Teste recomendado: confira ordenação do F10; abra Galaga e reinicie para verificar o último jogo; abra Space Manbow e observe se o reinício ocorre antes do primeiro quadro ou depois de começar o áudio. Se persistir, será necessário capturar a causa do reset em uma compilação específica de diagnóstico.
