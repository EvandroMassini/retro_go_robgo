# Revisão de áudio robgo-audio-fidelity1

**Revisão histórica, retirada após relato de reinícios. Não descreve o firmware.bin atual. Veja [restauração](RESTAURACAO.md).**

Esta revisão altera somente o mixer/frontend fMSX. GPIOs, vídeo, teclas, taxa de 32 kHz, tarefas e buffers de saída permanecem iguais.

## Alterações

- **SCC:** a reprodução das ondas de 32 pontos não descarta mais frequências acima de 1 kHz. Um acumulador fracionário mantém a fase entre blocos; a média da onda durante cada amostra é calculada por somas prefixadas, evitando percorrer muitos pontos por amostra. Frequências a partir de metade da taxa de saída continuam excluídas.
- **Tons PSG/melódicos:** utiliza a integração da transição da onda quadrada, em lugar do atalho que zerava amostras próximas às transições. O atalho anterior podia silenciar completamente um tom de 8 kHz na saída de 32 kHz.
- **Ganho:** master de 64 para 96, equivalente a +3,52 dB na região linear, mantendo o controle Volume do usuário. Acima de 75% da escala, uma curva estática comprime os picos antes do limite PCM. Isso reduz recorte brusco, mas pode alterar o timbre em passagens fortes; não é uma promessa de áudio sem distorção.

## Validação e limites

Compilação fMSX com ESP-IDF 4.4.8 aprovada. Verificação numérica independente das equações: notas SCC de 220 a 15.000 Hz, comparação com seno de referência, integração direta de 1.000 ondas/fases aleatórias, continuidade entre blocos, regressão do tom de 8 kHz e limites/simetria/monotonicidade da curva de ganho.

Essas verificações numéricas não executam o firmware nem medem desempenho na placa. A integração acrescenta cálculo no mixer; comparar jogabilidade e timbre em Space Manbow e Galaga continua necessário. A emulação de envelope/ruído PSG não foi substituída por um novo núcleo. A síntese YM2413/FM continua simplificada e sua percussão incompleta permanece como limitação conhecida.

## Arquivo para teste

`dist/firmware.bin` contém **somente fMSX**, versão interna `robgo-audio-fidelity1`. Grave em **0x110000** apenas no mapa launcher+fMSX atual. Não é imagem completa nem deve ser gravado em 0x0.

`dist/firmware.json` identifica esse aplicativo. O `robgo-completo.bin` e seu `manifest.json` são artefatos da compilação completa anterior: não foram atualizados por este build isolado.

Para comparar, mantenha a mesma ROM, cena, saída de áudio, Volume, Speed=1×, FM synth e frame skip. Comece com volume moderado porque esta revisão tem ganho maior. Compare música e efeitos ausentes, e também eventual distorção/custo de desempenho.
