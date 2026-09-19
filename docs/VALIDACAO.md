# Validação da preparação para publicação

Em 19/09/2026, no Windows:

- `pio run -t check`: aprovado, ESP-IDF 4.4.8 e Xtensa GCC 8.4.0.
- `pio run` na árvore de desenvolvimento: aprovado; imagem completa de 1.769.728 bytes, versão `1.46-8-g4ced1-dirty`.
- Segunda compilação a partir somente dos fontes publicáveis, sem objetos/builds anteriores: aprovada. Sem histórico Git nessa cópia, a versão usa o fallback `robgo-dev`.
- Bootloader, tabela e bytes de launcher/fMSX conferidos nos offsets finais pelo wrapper; aplicativos dentro das partições de 1 MiB e 640 KiB.
- Links relativos conferidos; arte e FabGL presentes na seleção dos arquivos.
- `git diff --check`: aprovado após limpeza de espaços finais e normalização de linhas dos arquivos modificados.
- Configuração privada de ferramentas, imagens de build, ELFs e logs excluídos da seleção de publicação. O binário auxiliar de teclado T-Deck já versionado pelo upstream continua pertencendo ao histórico de outro alvo.

As extensões PlatformIO, C/C++ e Python já estavam instaladas no ambiente examinado. A integração não instala ESP-IDF automaticamente; veja BUILDING.md. Existem avisos de compilação herdados, inclusive variáveis de diagnóstico sem uso quando logs estão desligados.

Não houve gravação de placa, teste físico adicional, commit ou push nesta preparação. A execução no hardware mantém a validação anterior do mantenedor. Linux/macOS não foram testados nesta etapa.
