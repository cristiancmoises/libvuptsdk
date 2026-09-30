# libvuptsdk

[![Licença: Apache-2.0](https://img.shields.io/badge/Licen%C3%A7a-Apache--2.0-blue.svg)](LICENSE)
[English](README.md)

**SDK em C para criar, verificar e extrair arquivos Zupt.**

O lançamento por código-fonte **2.1.0-base.2** integra o motor de arquivos
**Zupt 5.2.10** e o **codec VaptVupt 2.65.13**. Fornece bibliotecas compartilhada
e estática, uma API C opaca e o módulo `vuptsdk-base` do pkg-config. Zupt, a
aplicação VaptVupt, o codec independente e este SDK continuam sendo projetos
separados.

Copyright 2026 Cristian Cezar Moisés. O código de primeira parte está sob a
[Apache-2.0](LICENSE); os avisos de terceiros preservados estão em [NOTICE](NOTICE).

## Lançamento e compatibilidade

| Artefato | Versão | Escopo |
|---|---|---|
| `libvuptsdk-base.so.2` / `libvuptsdk-base.a` | 2.1.0-base.2 | API de arquivos compilável; versões de ABI `ZUPTSDK_1.0`, `1.1`, `1.2` |
| `prebuilt/libvuptsdk.so.2.0.3` | 2.0.3 congelada | ABI completa histórica; excluída da instalação e dos pacotes de lançamento |

A versão base é um pré-lançamento. O binário congelado possui funções `easy_*`,
métricas e criptografia por streaming cujo código-fonte completo não está neste
repositório. Os bindings históricos usam esse binário e não são bindings da
base. Atualizações do código-fonte não atualizam o binário congelado.

Os novos arquivos usam o formato Zupt **1.6**, com trailer de integridade do
arquivo (AIT), e exigem um leitor atualizado. A leitura de arquivos antigos
confiáveis sem AIT exige uma opção explícita no contexto; consulte o
[guia de migração](doc/API_REFERENCE.pt-BR.md#compatibilidade-de-arquivos).

## Compilar, testar e instalar

Requisitos: compilador C11, GNU Make, binutils, pthreads, Python 3 para as
ferramentas de teste/pacote e `pkg-config` para compilar aplicações. O Zupt é necessário para extrair os downloads `.zupt`;
não é uma dependência de execução do SDK. Se o compilador não tiver o alias
`cc`, acrescente `CC=gcc` (ou `CC=clang`) a cada comando `make` e use esse
compilador no lugar de `cc` abaixo.

```sh
make -j4
make test
sudo make install
sudo ldconfig                 # cache de bibliotecas compartilhadas no Linux
```

O prefixo padrão é `/usr/local`; use `make install PREFIX=/seu/prefixo` para
outro destino. O header público é instalado em `include/libvuptsdk-base/`,
separado do SDK antigo.

```sh
pkg-config --modversion vuptsdk-base   # 2.1.0-base.2
cc doc/example.c $(pkg-config --cflags --libs vuptsdk-base) -o exemplo
./exemplo
```

O [exemplo](doc/example.c) compacta um buffer com VaptVupt, verifica o arquivo e
confere a extração byte a byte. A [referência da API](doc/API_REFERENCE.pt-BR.md)
explica posse de memória, arquivos criptografados, integração com backend e
migração.

Para executar AddressSanitizer e UndefinedBehaviorSanitizer:

```sh
make test-asan
make clean
make -j4                     # restaura a compilação normal antes da instalação
```

No x86-64, a compilação padrão usa a base da arquitetura.
`VV_SIMD_FLAGS=-mavx2` é uma escolha explícita que faz todo o artefato do codec
exigir AVX2. O suporte de execução em outras plataformas está limitado às
evidências de [AUDIT.md](AUDIT.md).

As verificações opcionais do modelo usam `make test-proof` com Bend 2.0.5. As
leis provam propriedades de contagem e soma em árvores modeladas; não provam o
SDK em C nem a criptografia. As medições do modelo em CPU estão em
[BENCHMARKS.md](BENCHMARKS.md).

## Baixar, verificar e extrair

Os lançamentos são publicados nos quatro [hosts do repositório](#repositórios-e-proveniência).
Os novos pacotes usam `.zupt` a partir da versão 2.1.0-base.1; lançamentos e
pacotes anteriores mantêm seus nomes e formatos originais. Os downloads
principais são:

- `libvuptsdk-base-2.1.0-base.2-src.zupt`: código-fonte, testes e documentação do produto.
- `libvuptsdk-base-2.1.0-base.2-linux-x86_64.zupt`: biblioteca Linux x86-64, header e instalador.
- `SHA256SUMS`, `SHA256SUMS.asc` e `release-key.asc`: checksums, assinatura destacada e chave pública de assinatura.

Compare a impressão digital da chave pública com uma cópia confiável antes de
importá-la:

```text
0CFA 43B9 AA96 42EA AF2B E983 C4C6 61C9 ECFB 46E8
```

```sh
gpg --show-keys --with-fingerprint release-key.asc
gpg --import release-key.asc
gpg --verify SHA256SUMS.asc SHA256SUMS
sha256sum --ignore-missing --check SHA256SUMS
```

Confirme que cada arquivo baixado apresenta `OK`. Depois, use o Zupt 5.2.10 ou
um leitor mais novo compatível. Use um destino novo; a extração recusa
substituir arquivos existentes. As opções de extração devem vir antes do nome
do arquivo:

```sh
zupt test libvuptsdk-base-2.1.0-base.2-src.zupt
zupt extract -o ./source libvuptsdk-base-2.1.0-base.2-src.zupt
cd source/libvuptsdk-base-2.1.0-base.2
make -j4
make test
```

Se o Zupt 5.2.10 indicar erro de permissão no caminho de saída, use a
[alternativa com diretório temporário](doc/TROUBLESHOOTING.md#zupt-529-output-directory-permissions--permissões-do-diretório-de-saída).

Para o pacote binário, confira o checksum como acima e execute:

```sh
zupt test libvuptsdk-base-2.1.0-base.2-linux-x86_64.zupt
zupt extract -o ./binary libvuptsdk-base-2.1.0-base.2-linux-x86_64.zupt
cd binary/libvuptsdk-base-2.1.0-base.2-linux-x86_64
sudo sh install.sh
```

O binário exige **Linux x86-64 e glibc 2.34 ou superior**; foi testado com
glibc 2.41. O instalador usa `/usr/local` por padrão e aceita a variável
`PREFIX`. Compile a partir do código-fonte se esses requisitos não
corresponderem ao seu sistema. Os scripts
`packaging/build-deb.sh` e `packaging/build-rpm.sh` também geram candidatos a
pacotes Debian e RPM.

O checksum detecta corrupção; a assinatura GPG verificada identifica quem
assinou a lista de checksums. O selo de assinatura de um host Git é um recurso
separado. O contêiner `.zupt` inclui identificador aleatório e data de criação;
reconstruir o mesmo conteúdo não implica obter bytes idênticos no arquivo.

Para criar novos pacotes de lançamento a partir de um checkout Git:

```sh
make dist
make dist-binary
make test-package
```

Os comandos exigem Python 3 e Zupt; o pacote binário também usa `strip` e
`readelf`, do binutils. Selecione outra CLI com
`ZUPT=/caminho/para/zupt make dist`. O empacotador usa VaptVupt, modo sólido e
nível 9. O pacote-fonte inclui arquivos rastreados pelo Git; adicione novos
arquivos ao índice antes de empacotar. Um pacote-fonte extraído pode ser
reexportado sem Git enquanto os checksums de `SOURCE-MANIFEST.json` coincidirem;
use um checkout Git para empacotar código modificado. O gate binário rejeita
RPATH/RUNPATH e os testes de pacote comparam o conteúdo depois da extração.

## Integração com backend e segurança

Use um contexto por tarefa concorrente, configure o orçamento de extração com
`zuptsdk_ctx_set_max_decompressed()` e libere os buffers retornados com
`zuptsdk_free()`. O teto padrão de extração é 16 GiB; escolha um limite menor
adequado ao serviço. Tamanho da entrada, memória do processo, tempo de CPU e
espaço temporário em disco precisam de limites próprios. A API de I/O por
callbacks usa arquivos temporários e não implementa streaming de memória
constante.

A API por código-fonte oferece arquivos sem criptografia, com senha PBKDF2 e
com chave híbrida ML-KEM-768/X25519. Os checksums de arquivos sem criptografia
não autenticam o remetente. Consulte [SECURITY.md](SECURITY.md) para o escopo
criptográfico e as limitações conhecidas. Comunique vulnerabilidades de forma
privada para **zupt@riseup.net**.

## Repositórios e proveniência

| Projeto ou host | Local |
|---|---|
| SDK, principal | [git.securityops.co](https://git.securityops.co/cristiancmoises/libvuptsdk) |
| Mirror do SDK | [GitHub](https://github.com/cristiancmoises/libvuptsdk) |
| Mirror do SDK | [Codeberg](https://codeberg.org/berkeley/libvuptsdk) |
| Mirror do SDK | [git.securityops.com.br](https://git.securityops.com.br/cristiancmoises/libvuptsdk) |
| Aplicação VaptVupt | [vaptvupt](https://git.securityops.co/cristiancmoises/vaptvupt) |
| Codec independente | [vaptvupt-codec](https://git.securityops.co/cristiancmoises/vaptvupt-codec) |

Revisões importadas:

- Zupt **5.2.10**: `3b3b8f494b4bdd3b74aab60388eef1694ef316f8`.
- Codec VaptVupt **2.65.13**: `e30dc9329be7cf9f233b1ac0b1fc9ed31f530391`.

O SDK mantém adaptações para sua ABI pública e compilação. [NOTICE](NOTICE)
registra proveniência e obrigações de terceiros; o binário congelado mantém
seu licenciamento histórico. Consulte [CHANGELOG.md](CHANGELOG.md),
[AUDIT.md](AUDIT.md), [BENCHMARKS.md](BENCHMARKS.md) e a
[solução de problemas](doc/TROUBLESHOOTING.md).
