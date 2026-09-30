# Referência da API libvuptsdk-base

[English](API_REFERENCE.md)

Esta referência vale para o pré-lançamento compilável
`2.1.0-base.2` e para o header instalado
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

## Política do codec incorporado

O wrapper mapeia os níveis 1–2 para `ULTRA_FAST`, 3–7 para `BALANCED` e 8–9
para `EXTREME`. Os modos balanceado e extremo habilitam o filtro BCJ automático.
`format_v2=0` é a política de seleção automática do codec; não força todos os
frames a usar um formato antigo. Use leitores atuais para novos arquivos.

O checksum interno do frame é desativado porque o bloco do contêiner tem sua
própria verificação de integridade. A compactação também decodifica o frame
candidato e compara seus bytes com a entrada; se o candidato for rejeitado,
o chamador pode armazenar o bloco sem compactação. As funções internas `vvz_*`
não são API pública e não devem ser usadas sem as verificações do contêiner.

## Fluxo de arquivo em memória

1. Crie contexto e opções.
2. Chame `zuptsdk_compress_buffer()` com nome lógico e bytes de entrada.
3. Valide o resultado com `zuptsdk_verify()`.
4. Recupere um único arquivo com `zuptsdk_extract_buffer()`.
5. Libere archive e saída com `zuptsdk_free()`.

`zuptsdk_compress_files()` recebe caminhos do sistema de arquivos;
`zuptsdk_extract_to_dir()` extrai todas as entradas seguras. Consulte o exemplo
compilável em [example.c](example.c). Arquivos verificados são publicados sem
substituir destinos existentes: conflitos com arquivo, link simbólico ou FIFO
fazem a operação falhar e preservam o alvo existente. Use um diretório de
extração novo. Essa regra vale por arquivo, não como uma transação única para
todo o conteúdo.

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
sólida malformada, metadados, limite de extração, trailers de integridade,
testes do codec, licenciamento por arquivo e sanitizers. Notificações por callback, operações de
disco e execução em Windows/macOS ainda não fazem parte desse gate. Restauração
de disco é destrutiva e deve ser testada somente em imagens descartáveis ou
máquinas virtuais.


## Exemplo de arquivo em memória

```c
#include <string.h>
#include <zuptsdk.h>

int main(void)
{
    static const unsigned char entrada[] = "Conteudo do arquivo VaptVupt";
    zuptsdk_ctx_t *ctx = NULL;
    zuptsdk_options_t *opts = NULL;
    unsigned char *arquivo = NULL, *saida = NULL;
    size_t tamanho_arquivo = 0, tamanho_saida = 0;
    int rc = zuptsdk_ctx_create(&ctx);

    if (rc == 0) rc = zuptsdk_options_create(&opts);
    if (rc == 0) rc = zuptsdk_options_set_codec(opts, ZUPTSDK_CODEC_VAPTVUPT);
    if (rc == 0) rc = zuptsdk_compress_buffer(
        ctx, opts, "conteudo.txt", entrada, sizeof(entrada) - 1,
        NULL, NULL, &arquivo, &tamanho_arquivo);
    if (rc == 0) rc = zuptsdk_verify(ctx, arquivo, tamanho_arquivo, NULL, NULL);
    if (rc == 0) rc = zuptsdk_extract_buffer(
        ctx, arquivo, tamanho_arquivo, NULL, NULL, &saida, &tamanho_saida);

    int ok = rc == 0 && tamanho_saida == sizeof(entrada) - 1 &&
             memcmp(entrada, saida, tamanho_saida) == 0;
    zuptsdk_free(saida);
    zuptsdk_free(arquivo);
    zuptsdk_options_destroy(opts);
    zuptsdk_ctx_destroy(ctx);
    return ok ? 0 : 1;
}
```

Compile com `pkg-config`, como indicado no início desta referência.

## Arquivos criptografados

A versão por código-fonte oferece arquivos com senha PBKDF2 e com chave
híbrida ML-KEM-768/X25519. O wrapper seleciona PBKDF2 explicitamente; o suporte
opcional a Argon2 do projeto original não é uma configuração da API base.
Passe um buffer seguro de senha ou um handle de chave destinatária às
operações de arquivo e libere-o com a função de destruição correspondente.
A interface completa `easy_*` permanece exclusiva do binário congelado.
Consulte [SECURITY.md](../SECURITY.md) para os limites criptográficos.

## Compatibilidade de arquivos

A versão 2.1.0-base.2 grava o formato Zupt 1.6, com trailer de integridade do
arquivo (AIT) depois do rodapé e preâmbulos de bloco autenticados nos arquivos
criptografados. Use um leitor atualizado para novos arquivos; o binário
congelado 2.0.3 não é um leitor substituto compatível.

O contexto rejeita arquivos sem AIT por padrão. Para migrar um arquivo antigo
**confiável**, crie um contexto dedicado e habilite explicitamente:

```c
int rc = zuptsdk_ctx_set_allow_legacy_no_ait(ctx, 1);
/* Confira rc e verifique/extraia somente o arquivo legado confiavel. */
```

O setter aceita apenas 0 ou 1 e é exportado em `ZUPTSDK_1.2`. Zero é o padrão.
A opção permite a ausência do trailer; ela não desativa a verificação de um
trailer presente. Prefira regravar o conteúdo migrado no formato atual e manter
a opção desligada para entradas não confiáveis.

O AIT protege o header serializado e o prefixo do rodapé. Em arquivos
criptografados, usa HMAC-SHA256 com a chave MAC do arquivo; em arquivos sem
criptografia, usa XXH64 apenas para detectar corrupção. Os blocos de dados
possuem verificações próprias. Checksum sem criptografia e leitura de metadados
não comprovam quem criou um arquivo. As assinaturas de download são assinaturas
GPG separadas sobre `SHA256SUMS`, descritas no
[README](../README.pt-BR.md#baixar-verificar-e-extrair).

## Integração com um serviço backend

Use a API por código-fonte por meio de um módulo nativo C/C++ ou de um adaptador
FFI vinculado a `vuptsdk-base`. Os bindings existentes de Python, Node.js, Go e
Rust usam a ABI completa histórica e não podem ser redirecionados para esta
biblioteca como uma atualização direta. O `zuptsdk.h` instalado é o contrato
público; símbolos internos `vvz_*` e do motor ficam ocultos e não são pontos
de integração.

Um ciclo prático de requisição é:

1. Limite o tamanho do upload antes de passá-lo ao SDK. Crie um contexto para
   a tarefa e configure threads e teto de bytes descompactados.
2. Execute compactação e extração, que são bloqueantes, em uma fila limitada
   de workers. Considere as threads do SDK ao dimensionar a concorrência do
   próprio serviço.
3. Use um diretório novo, controlado pela aplicação, para cada extração e
   quotas separadas para disco temporário, tempo de CPU e memória do processo.
   O teto de descompactação não limita a entrada nem a memória.
4. Confira cada retorno. Trate `ZUPTSDK_ERR_TOO_LARGE`, erros de autenticação
   e entrada malformada como falhas; apresente mensagens da aplicação ao
   cliente e registre `zuptsdk_last_error_detail()` nos logs adequados.
5. Publique a saída somente depois do sucesso. Em caso de falha, descarte o
   diretório da requisição; a extração não é uma transação única para todas
   as entradas do arquivo.
6. Libere buffers com `zuptsdk_free()` e destrua opções, chaves, buffers de
   senha e contexto. Configure um alocador global personalizado apenas uma vez,
   no início do processo, antes de chamar o SDK.

O exemplo em memória serve para dados de tamanho limitado. Para arquivos
maiores, use a API de sistema de arquivos. O I/O por callbacks ainda usa
arquivos temporários e pode carregar buffers na memória; registrar callbacks
de progresso e log ainda não produz notificações. Não compartilhe um contexto
entre chamadas simultâneas. Este lançamento não afirma validação abrangente
com detector de corridas entre contextos separados.

## Licença

Copyright 2026 Cristian Cezar Moisés. O código de primeira parte e esta
documentação usam a [Apache-2.0](../LICENSE). Preserve os avisos de terceiros
em [NOTICE](../NOTICE). As referências ao binário e aos bindings históricos
não alteram a licença do binário congelado nem fornecem o código de sua API
adicional.
