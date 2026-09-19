# Publicação e manutenção

Origem: [ducalex/retro-go](https://github.com/ducalex/retro-go), base registrada `4ced120669750ca7228fd0414211430c1d923166`. Destino: [EvandroMassini/retro_go_robgo](https://github.com/EvandroMassini/retro_go_robgo). Placa RobGo_RG criada por **Rodolfo Guerra**. Documentação original em `README.upstream.md`; histórico upstream em `CHANGELOG.md`.

## Conteúdo da publicação

Inclua fontes e arquivos novos do alvo RobGo, cópia FabGL com licença/UPSTREAM, documentação, exemplos, temas, imagem `themes/default/background_robgo.png`, `platformio.ini`, `tools/platformio`, wrapper de build, exemplo JSON e configurações portáveis `.vscode`/`robgo.code-workspace`.

`ps2-keyboard-test` é um auxiliar histórico independente; não adiciona testes ao firmware normal. Seu `platformio.ini` está vazio nesta revisão e não deve ser usado como configuração do emulador. Para compilar o firmware mantido, abra a raiz.

O `.gitignore` exclui builds, imagens geradas, `dist`, `.pio`, logs, ELFs, mapas, configurações SDK geradas e `tools/idf.local.json`. Scripts VSCode com caminhos privados também ficam excluídos. Não inclua BIOS, jogos, saves pessoais, ferramentas instaladas ou conteúdo de cartão. Binários devem ser anexos da release.

```sh
git status --short
git diff --check
git diff --stat
```

Revise também os arquivos novos, que não aparecem em `git diff` antes de adicionados. Não substitua os links de autoria/histórico de dependências pelo endereço do fork.

## Remotos

Confira `git remote -v`. Se `origin` ainda aponta ao original e não há `upstream`, preserve-o e configure seu fork:

```sh
git remote rename origin upstream
git remote add origin https://github.com/EvandroMassini/retro_go_robgo.git
```

Se o destino já está correto, não repita. Faça commit dos arquivos revisados e envie a branch desejada. Preserve o histórico. A relação de fork visível no GitHub depende de criar/usar um fork no próprio GitHub, não apenas de mudar o remoto. Esta preparação local não realiza push.

## Release

1. Revise e faça commit/tag para identificar a versão.
2. Execute `pio run -t check` e `pio run`.
3. Teste SD, teclado, GPIOs da revisão física, ROM, atalhos F1/F9/F10/F11/F12, pasta `..`, save/load, áudio e proporções de monitor.
4. Anexe `dist/robgo-completo.bin` e `manifest.json`; preserve os ELFs correspondentes para diagnóstico.
5. Informe **ESP32 clássico, 4 MB, endereço 0x0**, pinagem efetiva, BIOS necessárias, alterações, limites e SHA-256.

Não trate a identificação `dirty` como única. Não apresente resultados de jogos isolados como garantia geral de FPS. O build validado não substitui testes de todas as revisões RobGo nem de todos os títulos.

## Licenças

Preserve `COPYING` e avisos dos componentes. O fMSX contém condições de distribuição não comercial e pedido de notificação ao autor sobre modificações. FabGL mantém licença própria. A licença da raiz não substitui esses termos. Esta preparação não concede novas permissões sobre BIOS, jogos ou marca da placa.

## Mapa dos fontes

| Caminho | Responsabilidade |
|---|---|
| `components/retro-go/targets/robgo-rg/config.h` | GPIOs e capacidades. |
| `components/retro-go/targets/robgo-rg/robgo.cpp` | VGA, PS/2 e perfis do bootl.rc. |
| `components/retro-go/targets/robgo-rg/sdkconfig` | Defaults IDF de builds limpos. |
| `fmsx/main/main.c` | Entrada, áudio, BIOS e opções fMSX. |
| `launcher/main/applications.c` | Jogos e pasta configurável. |
| `components/retro-go/rg_gui.c` | Opções e menus salvar/carregar. |
| `fmsx/components/fmsx/src/fMSX/Menu.c` | F10. |
| `themes/default/background_robgo.png`, `launcher/main/images.c` | Arte e array incorporado. Alterar só o PNG não atualiza automaticamente o array C. |
| `tools/build_robgo.py`, `rg_tool.py`, `tools/mkfw.py` | Build, merge e verificação. |
