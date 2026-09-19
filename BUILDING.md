# Desenvolvimento e compilação — RobGo_RG

Este guia destina-se ao **ESP32 clássico WROVER, flash de 4 MB e PSRAM**. A imagem contém dois aplicativos: launcher e fMSX. Não selecione ESP32-S3 ou Arduino.

## Dependências e extensões

| Ferramenta | Uso |
|---|---|
| Git | Fontes, histórico e versão. |
| ESP-IDF **4.4.8** | Framework usado nesta adaptação. |
| Ferramentas do instalador IDF | Xtensa GCC 8.4.0, CMake, Ninja, ULP e esptool. |
| Python do ambiente virtual IDF | Dependências de compilação; Python 3.11 foi usado no ambiente validado. |
| VSCode | Abrir a raiz ou `robgo.code-workspace`. |
| PlatformIO IDE / Core 6.1+ | Acionar a plataforma local deste repositório. |

FabGL **1.0.9**, fMSX/EMULib/Z80 e bibliotecas Retro-Go já estão nos fontes. **Não adicione outra FabGL em `lib_deps`**: a cópia em `components/fabgl` contém alterações locais e integração CMake. Driver, FATFS, JSON, NVS e demais componentes são fornecidos pelo IDF. Não instale bibliotecas Arduino. BIOS/jogos não são dependências da compilação.

As extensões recomendadas em `.vscode/extensions.json` são `platformio.platformio-ide`, `ms-vscode.cpptools` e `ms-python.python`. A extensão ESP-IDF da Espressif é opcional para quem prefere seu terminal. Não atualize para IDF 5.x sem portar e validar os drivers/FabGL.

## Instalação

```sh
git clone https://github.com/EvandroMassini/retro_go_robgo.git
cd retro_go_robgo
```

Instale ESP-IDF 4.4.8 e ferramentas ESP32 **fora do repositório**. Pode usar o instalador Windows ou obter a tag com submódulos:

```sh
git clone --branch v4.4.8 --recursive https://github.com/espressif/esp-idf.git esp-idf-v4.4.8
```

Na pasta do IDF, use `install.bat esp32` (Windows) ou `install.sh esp32` (Linux/macOS) e ative seu ambiente. Consulte os pré-requisitos no [guia oficial 4.4.8](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/get-started/index.html). Este projeto não baixa nem troca automaticamente seu IDF.

## Caminhos locais

Abra o VSCode a partir de um ambiente IDF 4.4.8 ativado **ou** copie `tools/idf.local.example.json` para `tools/idf.local.json` e ajuste:

```json
{
  "IDF_PATH": "C:/esp/esp-idf-v4.4.8",
  "IDF_TOOLS_PATH": "C:/esp/tools",
  "IDF_PYTHON_ENV_PATH": "C:/esp/tools/python_env/idf4.4_py3.11_env",
  "python": "C:/esp/tools/python_env/idf4.4_py3.11_env/Scripts/python.exe"
}
```

O nome do ambiente depende da instalação. Em Linux/macOS, use seu caminho de `bin/python`. O JSON local é ignorado pelo Git. O wrapper ativa ferramentas via `idf_tools.py` e verifica a versão do IDF.

## PlatformIO

O `platformio.ini` usa **`platform = symlink://tools/platformio`**, uma plataforma local que chama o build original ESP-IDF através de `tools/build_robgo.py` e `rg_tool.py`. **Não é uma conversão para Arduino nem utiliza o `framework-espidf` do registry.** Preserva as duas aplicações, a FabGL e a cadeia de ferramentas já validada.

```sh
pio run -t check
pio run
```

`check` verifica IDF, dependências Python e compilador. O build compila launcher/fMSX, atualiza o bootloader, combina a imagem e verifica os bytes nos offsets finais. Se `pio` não estiver no PATH, use o terminal PlatformIO do VSCode. No primeiro uso, o Core pode baixar ferramentas próprias, como SCons.

| Saída | Finalidade |
|---|---|
| `dist/robgo-completo.bin` | Imagem completa para gravar em **0x0**. |
| `dist/manifest.json` | Versão, tamanho, offsets e SHA-256. |
| `dist/launcher.elf`, `dist/fmsx.elf` | Símbolos da compilação para diagnóstico. |
| `retro-go_<versao>_robgo-rg.img` | Mesmos bytes da imagem completa gerada pelo empacotador. |
| `launcher/build`, `fmsx/build` | Objetos e bancos de comandos do compilador IDF. |

As opções genéricas `board`, `build_flags` e `lib_deps` do PlatformIO não controlam este build. GPIOs ficam nos fontes do alvo e opções do IDF em `components/retro-go/targets/robgo-rg/sdkconfig`. Para rebuild limpo, no terminal IDF:

```sh
python rg_tool.py --target robgo-rg clean launcher fmsx
pio run
```

Isso remove os objetos e configurações SDK geradas desses aplicativos. Transfira alterações deliberadas de `launcher/sdkconfig`/`fmsx/sdkconfig` para os defaults versionados antes de limpar. `pio run -t clean` limpa apenas metadados próprios do PlatformIO. Não execute builds simultâneos: há arquivos auxiliares compartilhados.

## VSCode e IntelliSense

Abra a raiz ou `robgo.code-workspace`, instale as extensões recomendadas e selecione um Python em **Python: Select Interpreter**. O JSON local pode apontar explicitamente ao Python do IDF.

- **Ctrl+Shift+B**: “RobGo: compilar imagem completa”.
- **Terminal → Run Task → RobGo: verificar ESP-IDF**: verifica ambiente.
- Terminal PlatformIO: `pio run`, `pio run -t check` e upload explícito.
- **C/C++: Select a Configuration**: escolha fMSX ESP-IDF ou Launcher ESP-IDF.

O C/C++ lê `compile_commands.json` do build real. Compile uma vez antes de esperar resolução completa de headers. A reindexação automática PlatformIO foi desativada para não substituir essas informações por um projeto de aplicativo único. Debug JTAG e testes unitários PlatformIO não foram configurados.

## Build sem PlatformIO

```sh
python tools/build_robgo.py check
python tools/build_robgo.py build
```

No terminal IDF também funciona `python rg_tool.py --target robgo-rg build-img launcher fmsx`. O comando direto gera `.img`; o wrapper acrescenta `.bin`, ELFs e manifesto. O bootloader sempre passa pelo build incremental, evitando reutilização desatualizada depois de alterar `sdkconfig`.

## Gravação e merge

No **ESP32 Flash Download Tool**, selecione ESP32, flash de 4 MB e grave **somente `dist/robgo-completo.bin` em 0x0**. Bootloader é apenas uma parte dessa imagem.

```sh
pio run -t upload --upload-port COM5
```

Alternativa com Python: `python tools/build_robgo.py upload --port COM5 --baud 460800`. Em Linux, use a porta apropriada, como `/dev/ttyUSB0`. Pode definir `upload_port` e `upload_speed` no INI. Upload PlatformIO depende do build; upload direto pelo wrapper usa o `.bin` já gerado. Compilar não grava a placa.

| Conteúdo dentro do `.bin` | Offset | Partição do aplicativo |
|---|---:|---:|
| Bootloader | `0x1000` | — |
| Tabela de partições | `0x8000` | — |
| Launcher | `0x10000` | 1 MiB |
| fMSX | `0x110000` | 640 KiB |

Não some esses offsets ao `.bin` completo. Não grave `fmsx.bin` em 0x0 nem siga o `app-flash` impresso pelo build de cada aplicativo: ele usa uma tabela provisória. O `partitions.csv` da raiz só atende ao build; o empacotador cria a tabela final multiaplicativo. `tools/mkfw.py` já está no repositório; os binários adicionais necessários ao merge são gerados pelo build, não precisam ser baixados.

## Versão e release

A versão vem de `git describe --tags --abbrev=5 --dirty --always`. Dois binários diferentes podem compartilhar o texto `dirty`; use também o SHA-256. Para release, faça commit/tag ou defina `PROJECT_VER` antes do build. Use texto curto e seguro para nome de arquivo, sem barras.

BIOS/SD não são necessários para compilar, mas são necessários para uso conforme o README. Não há logs periódicos na versão normal. Preserve os ELFs junto ao manifesto da versão distribuída.

Referências: [plataformas customizadas PlatformIO](https://docs.platformio.org/en/latest/platforms/creating_platform.html), [configuração PlatformIO](https://docs.platformio.org/en/latest/projectconf/index.html), [build ESP-IDF 4.4.8](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-guides/build-system.html).
