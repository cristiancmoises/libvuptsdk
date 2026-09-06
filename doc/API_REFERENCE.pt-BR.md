# Referência da API libvuptsdk-base

[English](API_REFERENCE.md)

Esta referência vale para o pré-lançamento compilável
`2.0.4-base.1` e para o header instalado
`libvuptsdk-base/zuptsdk.h`. O header é a fonte autoritativa para assinaturas e
regras de posse de memória.

O binário congelado `libvuptsdk.so.2.0.3` possui funções adicionais `easy_*`,
métricas e criptografia por streaming cujo código-fonte não está neste
repositório. Essas funções não são instaladas nem empacotadas pela versão base.

## Compilação e erros

```sh
cc aplicativo.c $(pkg-config --cflags --libs vuptsdk-base) -o aplicativo
```

As funções retornam `ZUPTSDK_OK` (zero) ou um valor negativo de
`zuptsdk_error_t`. Use `zuptsdk_strerror()` para a descrição estável e
`zuptsdk_last_error_detail()` para o detalhe da thread atual. Os erros mais
importantes na leitura de arquivos não confiáveis são:

| Valor | Nome | Significado |
|---:|---|---|
| -1 | `ZUPTSDK_ERR_INVALID_ARG` | Argumento inválido |
| -2 | `ZUPTSDK_ERR_NO_MEMORY` | Falha de alocação |
| -3 | `ZUPTSDK_ERR_IO` | Falha de arquivo ou callback |
| -4 | `ZUPTSDK_ERR_BAD_ARCHIVE` | Arquivo inválido ou truncado |
| -5 | `ZUPTSDK_ERR_BAD_PASSWORD` | Falha de autenticação com senha |
| -7 | `ZUPTSDK_ERR_BAD_MAC` | Tag de autenticação incorreta |
| -8 | `ZUPTSDK_ERR_BAD_VERSION` | Versão de formato incompatível |
| -9 | `ZUPTSDK_ERR_BAD_CHECKSUM` | Checksum incorreto |
| -14 | `ZUPTSDK_ERR_UNSUPPORTED` | Recurso indisponível nesta compilação |
| -16 | `ZUPTSDK_ERR_PATH_TRAVERSAL` | Caminho inseguro no arquivo |
| -17 | `ZUPTSDK_ERR_TOO_LARGE` | Saída ultrapassa o limite |
| -18 | `ZUPTSDK_ERR_CRYPTO_FAIL` | Falha de primitiva criptográfica |
| -99 | `ZUPTSDK_ERR_INTERNAL` | Invariante interna falhou |

Libere memória devolvida por ponteiros de saída com `zuptsdk_free()`. Objetos
opacos usam a função `*_destroy()` correspondente.

## Contexto e limite de extração

```c
zuptsdk_ctx_t *ctx = NULL;
int rc = zuptsdk_ctx_create(&ctx);
if (rc == ZUPTSDK_OK)
    rc = zuptsdk_ctx_set_max_decompressed(ctx, 256ull * 1024 * 1024);
/* use o contexto */
zuptsdk_ctx_destroy(ctx);
```

O teto padrão de extração é 16 GiB. Zero remove o limite e não é recomendado
para entrada não confiável. O decoder verifica o total declarado antes de criar
arquivos de saída e confere novamente os bytes realmente decodificados. O
antigo `zuptsdk_options_set_max_decompressed()` continua na ABI, mas não
controla as funções de extração; use o setter do contexto.

Não use o mesmo contexto em chamadas simultâneas. Configure o alocador global
uma vez, antes das demais chamadas. Os callbacks de log e progresso são
armazenados, mas ainda não são invocados neste pré-lançamento.

## Opções de compactação

Crie opções com `zuptsdk_options_create()` e destrua-as com
`zuptsdk_options_destroy()`. Os codecs são `AUTO`, `VAPTVUPT`, `LZHP`, `LZH`,
`LZ` e `STORE`; os níveis válidos vão de 1 a 9. Escolha
`ZUPTSDK_CODEC_VAPTVUPT` explicitamente quando esse formato for obrigatório.

## Fluxo de arquivo em memória

1. Crie contexto e opções.
2. Chame `zuptsdk_compress_buffer()` com nome lógico e bytes de entrada.
3. Valide o resultado com `zuptsdk_verify()`.
4. Recupere um único arquivo com `zuptsdk_extract_buffer()`.
5. Libere archive e saída com `zuptsdk_free()`.

`zuptsdk_compress_files()` recebe caminhos do sistema de arquivos;
`zuptsdk_extract_to_dir()` extrai todas as entradas seguras. Consulte o exemplo
compilável em `doc/example.c`.

## I/O por callbacks

`zuptsdk_compress_stream()` e `zuptsdk_decompress_stream()` aceitam callbacks
de leitura e escrita. Nesta versão os dados passam por arquivos temporários com
permissão restrita: é uma interface por callbacks, não um streaming de memória
constante. A descompactação respeita o limite do contexto.

## Buffers seguros, chaves e metadados

Buffers seguros tentam usar `mlock()` e são zerados explicitamente na
destruição, mas limites do sistema podem impedir o bloqueio das páginas. Em
Linux testado, a chave privada salva recebe modo 0600 antes da primeira escrita,
um link simbólico no componente final é recusado e alvos que não sejam arquivos
regulares são rejeitados.

`zuptsdk_archive_info_read()` lê versão, data, UUID, tamanho, flags e a contagem
de blocos de um rodapé estruturalmente válido sem extrair o payload. O getter
antigo de contagem tem 32 bits e satura em `UINT32_MAX`.

## Limite atual de validação

Os testes de lançamento cobrem ciclo de vida, zeroização, ciclos de arquivo
VaptVupt sem criptografia, autenticado por senha e com chave híbrida, entrada
sólida malformada, metadados, limite de extração, 57 testes do codec,
licenciamento por arquivo e sanitizers. Notificações por callback, operações de
disco e execução em Windows/macOS ainda não fazem parte desse gate. Restauração
de disco é destrutiva e deve ser testada somente em imagens descartáveis ou
máquinas virtuais.
