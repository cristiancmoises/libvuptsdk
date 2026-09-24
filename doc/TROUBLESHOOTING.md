# libvuptsdk-base troubleshooting / Solução de problemas

Applies to / Aplica-se a **2.1.0-base.1**. See the
[English API reference](API_REFERENCE.md) or the
[referência em português](API_REFERENCE.pt-BR.md).

## Build and runtime linking / Compilação e carregamento

If `make` cannot find `cc`, select the installed compiler explicitly:
`make CC=gcc` or `make CC=clang`. Pass the same override to later test and
install commands and use that compiler for the examples below.

Se `make` não encontrar `cc`, selecione o compilador instalado explicitamente:
`make CC=gcc` ou `make CC=clang`. Repita a opção nos comandos de teste e
instalação e use esse compilador nos exemplos abaixo.

Use the base pkg-config module for both headers and libraries:

Use o módulo pkg-config da base para headers e bibliotecas:

```sh
cc app.c $(pkg-config --cflags --libs vuptsdk-base) -o app
```

For a custom installation prefix, point pkg-config to its metadata. On Linux,
`ldconfig` updates the system loader cache after a system installation; for an
isolated test, set the runtime library path for that invocation:

Para um prefixo personalizado, indique os metadados ao pkg-config. No Linux,
`ldconfig` atualiza o cache do carregador após a instalação no sistema; para
um teste isolado, configure o caminho da biblioteca nessa execução:

```sh
PKG_CONFIG_PATH=/opt/libvuptsdk/lib/pkgconfig pkg-config --cflags --libs vuptsdk-base
sudo ldconfig
LD_LIBRARY_PATH=/opt/libvuptsdk/lib ./app
```

If you ran `make test-asan`, rebuild normally before installing:

Se executou `make test-asan`, recompile normalmente antes de instalar:

```sh
make clean
make -j4
```

## Missing `easy_*` symbols / Símbolos `easy_*` ausentes

The base library does not implement the historical full-ABI convenience layer.
`make install` installs **the base library**. Adapt new applications to
`zuptsdk.h` and its archive functions. The existing bindings and
`prebuilt/libvuptsdk.so.2.0.3` belong to the frozen compatibility surface;
renaming or symlinking that binary does not upgrade it.

A biblioteca base não implementa a camada de conveniência da ABI completa
histórica. `make install` instala **a biblioteca base**. Adapte novas aplicações
às funções de arquivo de `zuptsdk.h`. Os bindings existentes e
`prebuilt/libvuptsdk.so.2.0.3` pertencem à compatibilidade congelada; renomear
esse binário ou criar um link simbólico não o atualiza.

## Archive failures / Falhas de arquivo

Use `zuptsdk_strerror(rc)` and `zuptsdk_last_error_detail()` after a failed call.
The same numeric code can mean something different in a legacy binding;
interpret it using the header for the library actually loaded.

Use `zuptsdk_strerror(rc)` e `zuptsdk_last_error_detail()` depois de uma falha.
Um código numérico pode ter outro significado em um binding antigo;
interprete-o com o header da biblioteca realmente carregada.

| Base error | English | Português |
|---|---|---|
| `ZUPTSDK_ERR_TOO_LARGE` | Output exceeds the context ceiling; review the request budget | Saída excede o teto do contexto; revise o orçamento da requisição |
| `ZUPTSDK_ERR_BAD_ARCHIVE` | Invalid, truncated or incompatible archive | Arquivo inválido, truncado ou incompatível |
| `ZUPTSDK_ERR_BAD_PASSWORD` / `BAD_MAC` | Wrong credentials, modified input or incompatible historical encryption | Credenciais incorretas, entrada alterada ou criptografia histórica incompatível |
| `ZUPTSDK_ERR_PATH_TRAVERSAL` | An entry has an unsafe extraction path | Uma entrada tem caminho de extração inseguro |
| `ZUPTSDK_ERR_IO` | Check permissions, free space and temporary storage | Confira permissões, espaço livre e armazenamento temporário |

Trusted older archives may lack AIT. Follow the explicit migration option in
the API reference; keep it disabled for untrusted requests. Format 1.6 output
needs an updated reader. A failed authentication check alone does not identify
which of the possible causes occurred.

Arquivos antigos confiáveis podem não ter AIT. Siga a opção explícita de
migração na referência da API; mantenha-a desativada para requisições não
confiáveis. O formato 1.6 exige um leitor atualizado. Uma falha de autenticação
isolada não identifica qual das causas possíveis ocorreu.

Extraction refuses to overwrite existing destinations, including regular
files, symlinks and FIFOs. Use a fresh directory when retrying; if an archive
fails after earlier entries succeeded, discard that request's output directory.

A extração recusa sobrescrever destinos existentes, inclusive arquivos comuns,
links simbólicos e FIFOs. Use um diretório novo ao tentar novamente; se um
arquivo falhar depois de extrair entradas anteriores, descarte o diretório de
saída daquela requisição.

## Memory and resource use / Memória e recursos

Free returned buffers with `zuptsdk_free()`, and pair each opaque object's
creation with its matching destroy function. Do not substitute the C library's
`free()` when SDK allocators may differ. The output ceiling does not limit
input buffers or temporary disk use. Memory locking is best-effort and may be
refused by OS resource limits.

Libere buffers retornados com `zuptsdk_free()` e use a função de destruição
correspondente a cada objeto opaco criado. Não substitua pela `free()` da
biblioteca C quando os alocadores do SDK puderem ser diferentes. O teto de
saída não limita buffers de entrada nem disco temporário. O bloqueio de
memória é uma tentativa e pode ser recusado por limites do sistema.

## Downloads / Downloads

Verify the GPG signature and checksums before extracting. With the Zupt CLI,
options come before the archive: `zupt extract -o destination archive.zupt`.
See the READMEs for exact filenames, fingerprint and source/binary instructions.
New packages use `.zupt` starting with 2.1.0-base.1; old release formats remain
unchanged.

Verifique a assinatura GPG e os checksums antes da extração. Na CLI Zupt, as
opções vêm antes do arquivo: `zupt extract -o destino arquivo.zupt`. Os READMEs
contêm nomes exatos, impressão digital e instruções para fonte e binário. Os
novos pacotes usam `.zupt` a partir da versão 2.1.0-base.1; os formatos dos
lançamentos antigos continuam os mesmos.

### Zupt 5.2.9 output-directory permissions / Permissões do diretório de saída

The standalone Zupt 5.2.9 CLI can fail when an output path has an ancestor that
allows traversal but not directory listing. The SDK fixes that path handling;
the separately installed CLI needs its own update. Until then, extract to a
private directory under an accessible path, then move the extracted package:

A CLI independente Zupt 5.2.9 pode falhar quando um ancestral do caminho de
saída permite travessia, mas não listagem. O SDK corrige esse tratamento; a CLI
instalada separadamente precisa de sua própria atualização. Enquanto isso,
extraia em um diretório privado sob um caminho acessível e depois mova o pacote:

```sh
extract_stage=$(mktemp -d /tmp/libvuptsdk-extract.XXXXXX)
zupt extract -o "$extract_stage" "$PWD/libvuptsdk-base-2.1.0-base.1-src.zupt"
mv "$extract_stage/libvuptsdk-base-2.1.0-base.1" ./
rmdir "$extract_stage"
```

Use a destination where that package directory does not already exist. Apply
the same staging approach to the binary bundle using its filename and root.

Use um destino onde o diretório desse pacote ainda não exista. Aplique o mesmo
procedimento ao pacote binário, usando o nome e o diretório raiz correspondentes.

## Reporting bugs / Relatar problemas

Include the output of `zuptsdk_version_string()`, operating system, compiler,
loaded library path and a minimal reproducer without secrets. For memory
errors, include `make test-asan` output when available.

Inclua a saída de `zuptsdk_version_string()`, sistema operacional, compilador,
caminho da biblioteca carregada e um exemplo mínimo sem segredos. Para erros
de memória, inclua a saída de `make test-asan` quando disponível.

Public issues / Problemas públicos:
[libvuptsdk issues](https://git.securityops.co/cristiancmoises/libvuptsdk/issues).
Security reports / Relatos de segurança: **zupt@riseup.net**, privately / em
particular.

Copyright 2026 Cristian Cezar Moisés. [Apache-2.0](../LICENSE).
