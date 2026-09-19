# Menus e comandos — RobGo_RG

Os nomes em inglês correspondem aos fontes; Language pode traduzir parte deles. Consulte também a [tabela completa de atalhos e teclado MSX](../README.md#teclas-e-navegação).

## Navegação comum

Setas selecionam; **Enter, Enter numérico ou Espaço** confirmam; **Esc ou Ctrl** representam voltar/cancelar. Em diálogos Retro-Go, esquerda/direita muda valores e cima/baixo muda linha. F11/F12 também podem fechar diálogos genéricos. O menu fMSX F10 usa outro sistema: nem todo comando de seu console genérico chega pelo backend ESP32. Use setas e confirmar/cancelar; não se promete PgUp/PgDn/Backspace como atalhos de navegação F10.

## Launcher e ações dos jogos

| Tecla | Ação |
|---|---|
| Setas / F9 / F10 no carrossel | Sistema anterior/próximo. |
| Enter/Espaço | Entrar no sistema ou abrir pasta/jogo. |
| Cima/baixo na lista | Mover uma linha. |
| Esquerda/direita na lista | Mover uma página. |
| F9 / F10 na lista | Aba anterior/próxima. |
| Esc/Ctrl | Subir pasta; na raiz, voltar ao carrossel. |
| F11 | Opções. |
| F12 / Magic button | About. |

O launcher mantém favoritos e recentes. A imagem deste fork contém somente launcher e fMSX, embora outros núcleos continuem no repositório upstream.

Ao selecionar jogo: **Resume game** escolhe um estado disponível; **New game** inicia sem save state; **Delete save** remove estado e pode perguntar pela SRAM persistida; a ação de **favorito** adiciona/remove da lista; **Properties** mostra informações e ações adicionais, incluindo exclusão do arquivo mediante confirmação. Opções podem estar desabilitadas por ausência de dados. Apagar save não é apagar ROM.

O ESP32 pode reiniciar ao trocar do launcher para a partição fMSX. Uma troca única é normal; reinícios repetidos em loop não são.

### F11 no launcher

| Opção | Uso |
|---|---|
| Volume | Nível de som. |
| Font type / Theme | Fonte e tema da interface. |
| Show clock / Timezone | Exibição do relógio e fuso; não habilita sincronização de rede. |
| Language | Idioma dos textos traduzidos. |
| Launcher options → Color theme | Paleta do launcher. |
| Preview | Nenhuma, prioridade capa/save, save/capa ou apenas um tipo. |
| Scroll mode | Center ou Paging. |
| Start screen | Auto, Carousel ou Browser. |
| Hide tabs | Mostrar/ocultar categorias. |
| Startup app | Launcher ou Last game. |

### F12 — About

Mostra Version, Date, Target e Website. **Reset settings** restaura configurações pelo mecanismo Retro-Go. **Build CRC cache** prepara cache de identificação de arquivos e pode demorar/acessar o SD. Debug e atualização online não ficam disponíveis neste alvo.

## F9 — teclado virtual durante o jogo

Mostra/oculta o teclado MSX. Setas selecionam; Enter/Espaço aciona; Esc/Ctrl fecha. O teclado físico continua encaminhado à matriz MSX. F9–F12 são reservados ao frontend. **F1–F5 são teclas MSX; F1 somente pausa jogos que atribuem pausa a essa tecla.**

## F10 — menu interno MSX

Na base restaurada, o seletor usa a ordem de listagem do FAT (pastas antes dos arquivos), sem ordenar alfabeticamente. O registro independente de último jogo pelo F10 foi revertido junto com a revisão experimental; veja [restauração](RESTAURACAO.md).

| Entrada | Função |
|---|---|
| Open file | Seletor para ROM, disco, estado ou outro formato reconhecido pelo núcleo. |
| Save state | Estado pelo mecanismo interno fMSX, distinto dos slots F12. |
| Hardware model | MSX1/TMS9918, MSX2/V9938, MSX2+/V9958; NTSC/PAL; RAM e VRAM. Pode reiniciar a máquina. |
| Input devices | Portas MSX 1/2 e autofire. |
| Cartridge slots | Cartuchos A/B e mapper. |
| Disk drives | Drives A/B e imagens de disco. |
| Cheats / Search cheats | Códigos e busca de valores em memória; recursos avançados herdados. |
| Fixed font | Alternar fonte fixa oferecida pelo núcleo. |
| All sprites | Alterar limite de sprites; pode mudar aparência em relação ao MSX real. |
| Patch DiskROM | Patches de acesso de disco fMSX. |
| Rewind tape | Rebobinar fita virtual carregada. |
| Reset emulator | Reiniciar MSX emulado. |
| Return to launcher | Sair para seleção; salve antes se desejar conservar progresso. |
| Resume game | Fechar menu e continuar. |

**Voltar de uma pasta:** selecione **`..`** e confirme. **Seta esquerda** também solicita a pasta anterior no seletor. **Seta esquerda** também solicita a pasta anterior no seletor. O fork cria essa entrada abaixo de `/sd`, mesmo quando o FAT/VFS não a lista. Esc/Ctrl cancela o seletor; não sobe uma pasta. O launcher usa Esc/Ctrl para subir, mas este navegador interno é diferente.

### Submenus F10

- **Input devices:** para cada porta Empty, Joystick, JoyMouse ou Mouse; alternâncias Autofire on SPACE, FIRE-A e FIRE-B. A ponte física de mouse alimenta a primeira porta (índice 0); dois mouses não foram validados.
- **Cartridge slots:** A/B; Load cartridge, Eject cartridge, Guess MegaROM mapper. Mappers manuais: Generic 8kB/16kB, Konami 5000h/4000h, ASCII 8kB/16kB, Konami GameMaster2 e Panasonic FMPAC. Trocar mapper pode reiniciar a máquina; comece pela detecção automática.
- **Disk drives:** A/B; Load disk, New disk, Eject disk, Save DSK image, Save FDI image. Reescrever imagem de disco não é salvar estado. Faça cópia dos discos antes de alterar arquivos importantes.
- **Cheats:** Enabled liga/desliga códigos; New cheat insere; selecionar um código existente o remove; Done conclui.
- **Search cheats:** Clear all watches limpa buscas; Add a new watch adiciona valor observado; Scan watches refaz a procura; See cheat codes mostra resultados; Done conclui. A busca oferece New value, 8bit/16bit, Constant, Changes by +1/-1/+N/-N e Search now. São recursos herdados; entrada de texto e todas as operações não foram validadas nessa placa.

### BASIC

Com BIOS apropriada, ejete os cartuchos A/B, remova mídias de autoboot e use Reset emulator para chegar ao BASIC. Não há botão dedicado “Boot BASIC” no launcher.

```basic
10 PRINT "OLA, ROBGO!"
20 GOTO 10
RUN
```

Pause no teclado físico corresponde a STOP do MSX. Persistir programas exige disco/fita virtual e comandos da BIOS ou save state; não há gravação automática de `.bas` no SD.

## F11 — Options durante o jogo

| Opção | Uso |
|---|---|
| Volume | Nível do DAC. |
| Scaling | Off mantém tamanho lógico; Fit preserva proporção; Full preenche; modo personalizado permite fator. |
| Factor | Fator da escala personalizada, visível quando aplicável. |
| Filter | Suavização; Off favorece pixels nítidos e menor custo. |
| Border | Borda gráfica quando disponível. |
| Speed | Velocidade relativa; afeta áudio/sincronização. Use 1× normalmente. |
| Monitor | 4:3/16:9/21:9; compensação de proporção fora do modo Full. |
| Emulator options → Input | Keyboard/Joystick herdado; teclado físico RobGo permanece na matriz MSX. |
| Crop | Recorte das bordas do quadro MSX. |
| Frame skip | 0%, 25%, 50%, 75% de quadros omitidos do desenho. |
| Auto frame skip | Adapta descarte ao tempo disponível. |
| FM synth | Síntese FM opcional; Off preserva PSG/SCC. |

As opções são persistidas no SD, não como parâmetros novos de `bootl.rc`. Menus podem silenciar áudio intencionalmente. Restart audio, Tone/DMA test, captura PCM, Audio out e Overclock não existem na interface normal RobGo.

## F12 / Magic — menu Retro-Go de jogo

- **Save & Continue**: salvar em slot e continuar.
- **Save & Quit**: salvar e voltar ao launcher.
- **Load game**: carregar estado.
- **Reset**: reiniciar pelo frontend.
- **Quit**: sair sem criar novo save state.

O seletor oferece **slots 0 a 3**, com disponibilidade/confirmações conforme o contexto. Não há netplay neste alvo.

## Teclado gráfico de texto Retro-Go

Separado do teclado virtual MSX: setas selecionam; Enter/Espaço insere; Esc/Ctrl apaga; F9 alterna layouts; F10 conclui; F11/F12 cancela.

## Fontes de referência

[PS/2 e atalhos](../components/retro-go/targets/robgo-rg/robgo.cpp), [launcher](../launcher/main/main.c), [ações dos jogos](../launcher/main/applications.c), [menus Retro-Go](../components/retro-go/rg_gui.c), [frontend fMSX](../fmsx/main/main.c), [menu F10](../fmsx/components/fmsx/src/fMSX/Menu.c), [seletor interno](../fmsx/components/fmsx/src/EMULib/Console.c).
