# libvuptsdk

[English](README.md) | Português do Brasil

`libvuptsdk` fornece uma ABI C para recursos criptográficos e de arquivo usados
por aplicações clientes. **Zupt é um consumidor existente e separado; seu nome
não é alterado por este projeto.** A aplicação VaptVupt é relacionada, mas não
é intercambiável com Zupt, com este SDK ou com o codec independente. O
repositório contém uma biblioteca base compilável a partir do código-fonte e
um binário completo antigo. Essas duas variantes não têm o mesmo escopo nem o
mesmo estado de atualização.

## Estado desta árvore

- Pré-lançamento compilável: **2.0.4-base.1**
  (`libvuptsdk-base.so.2`).
- Codec incorporado na compilação por código-fonte: **VaptVupt 2.65.11**,
  sincronizado do commit
  `1cc78bce90619dbf97e0ed1ad449c3c4f6329041`.
- O codec mantém sua licença `GPL-3.0-or-later`; ele não foi relicenciado para
  satisfazer a política de licenciamento do SDK.
- A versão completa congelada permanece **2.0.3** e é usada somente pelo alvo
  explícito `make legacy-test`.

## Duas bibliotecas, dois escopos

| Artefato | Origem | Conteúdo | Situação |
|---|---|---|---|
| `libvuptsdk-base.so` / `libvuptsdk-base.a` | Compilado deste repositório | Subconjunto `ZUPTSDK_1.0` e o limitador `ZUPTSDK_1.1`, incluindo o codec atual | Artefato atual de pré-lançamento |
| `libvuptsdk.so` | Arquivo em `prebuilt/` | ABI completa `ZUPTSDK_1.0` + `ZUPTSDK_2.1` | Binário x86-64 congelado; somente compatibilidade |

A árvore pública ainda não contém o código de algumas funções `easy_*` e de
outros símbolos exclusivos da biblioteca completa. Por isso, o binário em
`prebuilt/` não pode ser regenerado honestamente apenas com este repositório.
Não renomeie esse arquivo para uma versão nova nem afirme que ele contém o
codec 2.65.11. Ele não é instalado, empacotado nem anexado ao lançamento base.
Consulte [SECURITY.md](SECURITY.md) para as demais limitações.

## Compilar e testar

Requisitos básicos: compilador C11, GNU Make, binutils e uma implementação de
`pthread`.

```sh
make
make test
make test-asan
```

`make test` executa doze verificações públicas da variante compilada, inclusive
ciclos reais sem criptografia, com senha e com chave híbrida, 57 testes focados
do codec e a verificação das licenças por arquivo. `make test-asan` recompila a
variante com AddressSanitizer e UndefinedBehaviorSanitizer. Use `make
legacy-test` somente para verificar o binário 2.0.3 congelado.

Para produzir o arquivo-fonte determinístico:

```sh
make dist
```

O arquivo `libvuptsdk-base-2.0.4-base.1.tar.gz` inclui os avisos e textos de
licença do SDK e do codec, mas não inclui a biblioteca completa congelada. Os
scripts abaixo geram candidatos nativos separados, sem fingir que fornecem a
ABI completa:

```sh
packaging/build-deb.sh
packaging/build-rpm.sh
```

Os pacotes Debian se chamam `libvuptsdk-base2` e
`libvuptsdk-base-dev`; os RPMs usam `libvuptsdk-base` e
`libvuptsdk-base-devel`. Os scripts recusam uma biblioteca que contenha
RPATH/RUNPATH. Os arquivos anexados ao lançamento não são assinados por uma
distribuição; valide-os com `SHA256SUMS`.

No x86-64, a compilação padrão usa a base SSE2 da arquitetura e não força AVX2.
Definir `VV_SIMD_FLAGS=-mavx2` é uma opção explícita que faz todo o artefato do
codec exigir um processador com AVX2.

## Uso mínimo da ABI pública

```c
#include <stdio.h>
#include <zuptsdk.h>

int main(void)
{
    printf("libvuptsdk %s\n", zuptsdk_version_string());
    return 0;
}
```

Compile com os dados do `pkg-config`:

```sh
cc exemplo.c $(pkg-config --cflags --libs vuptsdk-base) -o exemplo
```

A referência da API está em
[doc/API_REFERENCE.md](doc/API_REFERENCE.md), com uma versão em português em
[doc/API_REFERENCE.pt-BR.md](doc/API_REFERENCE.pt-BR.md). Um exemplo mínimo
fica em [doc/example.c](doc/example.c).

As extrações da biblioteca base têm limite total padrão de 16 GiB. Ajuste-o
por contexto com `zuptsdk_ctx_set_max_decompressed()`; zero remove o limite e
não é recomendado para arquivos não confiáveis. O setter antigo no objeto de
opções é mantido apenas para compatibilidade e não controla a extração.

## Contrato do codec incorporado

O wrapper interno preserva o formato já usado pelos arquivos do SDK:

- níveis 1–2 usam o modo rápido, 3–7 usam o balanceado e 8–9 usam o extremo;
- blocos com tags do formato v2 permanecem desativados;
- o checksum interno do frame é omitido porque o contêiner do SDK armazena um
  XXH64 do bloco descomprimido e autentica o payload quando há criptografia;
- o decoder aceita os frames legados cobertos pelos testes de compatibilidade;
- os filtros BCJ continuam opt-in no codec e são invertidos automaticamente
  pelo decoder.

Quem chamar as funções internas `vvz_*` fora do fluxo normal do SDK precisa
fornecer uma verificação de integridade equivalente. Elas não fazem parte dos
headers instalados como ABI pública.

## Segurança

Leia [SECURITY.md](SECURITY.md) antes de implantar a biblioteca. O documento
descreve o modelo de ameaça, os algoritmos, a cobertura de testes e as
limitações conhecidas. Vulnerabilidades devem ser comunicadas de forma privada
para `zupt@riseup.net`.

Resultados de testes internos não substituem uma auditoria independente. A
biblioteca completa pré-compilada também não deve herdar automaticamente as
afirmações verificadas apenas na compilação por código-fonte.

## Licenças

O código de primeira parte do SDK identificado com
`AGPL-3.0-or-later OR LicenseRef-libvuptsdk-Commercial` está disponível sob a
AGPL ou sob um contrato comercial assinado separadamente. O aviso comercial
não é, por si só, uma licença.

Os arquivos incorporados do VaptVupt identificados com
`GPL-3.0-or-later` permanecem sob a GPL. Veja [NOTICE](NOTICE),
[LICENSE-AGPL-3.0](LICENSE-AGPL-3.0),
[LICENSE-GPL-3.0](LICENSE-GPL-3.0) e
[LICENSE-COMMERCIAL](LICENSE-COMMERCIAL).

## Repositórios

- Principal: <https://git.securityops.co/cristiancmoises/libvuptsdk>
- Mirror no GitHub: <https://github.com/cristiancmoises/libvuptsdk>
- Mirror no Codeberg: <https://codeberg.org/berkeley/libvuptsdk>
- Mirror em securityops.com.br:
  <https://git.securityops.com.br/cristiancmoises/libvuptsdk>
- Aplicação VaptVupt: <https://git.securityops.co/cristiancmoises/vaptvupt>
- Codec VaptVupt: <https://git.securityops.co/cristiancmoises/vaptvupt-codec>
