# Áudio — robgo-audio-quality2

Esta revisão retoma as correções de síntese depois de o usuário confirmar que a
imagem completa funciona com os jogos MSX2. Esse resultado aponta para uma
diferença na instalação da aplicação isolada, mas não prova um defeito específico
do bootloader externo.

## Alterações

- SCC: acumulador de fase Q16 e interpolação linear da tabela de 32 amostras.
  O caminho antigo descartava notas acima de aproximadamente 1 kHz na saída de
  32 kHz. O novo caminho aceita frequências abaixo de Nyquist (16 kHz), mantém
  a fase entre blocos e não divide por amostra. A interpolação suaviza a tabela,
  mas não é um filtro completo contra aliasing de todos os seus harmônicos.
- PSG: integração das transições da onda quadrada, substituindo a aproximação
  que podia cancelar inteiramente um tom de 8 kHz a 32 kHz. Há um pequeno custo
  adicional nas transições; o desempenho real precisa ser avaliado na placa.
- SCC: reinicialização de tipo, frequência e volume dos cinco canais, em vez
  de repetir a configuração de tipo apenas no primeiro canal.
- Ganho do mixer: 64 → 96, aproximadamente +3,52 dB na região linear.
  Compressão estática suave acima de 75% da escala para evitar recorte abrupto
  dos picos. Não é normalização automática; picos fortes alteram a forma de onda.
- O comando `tools/build_robgo.py build` passa a gerar também `dist/firmware.bin`,
  seu ELF e metadados, a partir da mesma aplicação incluída na imagem completa.

As alterações desta revisão não modificam GPIOs, DMA, taxa de amostragem,
escalonamento de tarefas, vídeo, menus ou configurações do SD. Logs de testes
não foram reativados.

## Limites conhecidos

O YM2413 desta versão continua sendo uma aproximação dos instrumentos FM através
do sintetizador genérico. Os timbres não equivalem à síntese OPLL completa;
a percussão FM encaminhada a `Drum()` não tem gerador PCM instalado. Portanto,
esta revisão corrige causas concretas de notas SCC/PSG ausentes, mas não garante
equivalência sonora com outro emulador. Uma implementação OPLL completa requer
integração e medição de custo no ESP32 antes de ser adotada.

## Validação e comparação

`python tools/test_audio_quality.py` verifica um modelo numérico do algoritmo:
notas SCC de 110 a 15999 Hz, continuidade entre blocos, índices/amplitudes,
tom PSG de 8 kHz e monotonicidade/simetria do limitador. Não executa o firmware
nem substitui testes de audição e desempenho no ESP32.

Para comparar, use o mesmo jogo, trecho, volume do menu, FM synth e frame skip.
Teste Space Manbow e Galaga, além de uma troca de jogos e reset. Comece com o
volume reduzido, pois o ganho interno aumentou. A confirmação de funcionamento
da imagem completa anterior não é validação desta nova revisão.

## Arquivos

- `dist/robgo-completo.bin`: imagem completa para endereço 0x0, com bootloader,
  tabela de partições, launcher e fMSX.
- `dist/firmware.bin`: somente a aplicação fMSX, sem tabela nem bootloader.
  Sua instalação depende do contrato e layout do bootloader que a recebe.
  Na imagem completa deste projeto, a aplicação está em 0x110000; esse endereço
  não deve ser presumido para um bootloader externo.
- `dist/manifest.json` e `dist/firmware.json`: versão, tamanho e SHA-256.
- `dist/firmware.elf`: símbolos correspondentes à aplicação.

O conteúdo de `firmware.bin` deve ser idêntico ao trecho correspondente do merge.
Como o usuário confirmou funcionamento com a imagem completa, ela é a opção
preferida para esta comparação de áudio.
