# cortex9-libxml2-strncat-length-wrap-model

Modelo de aritmética de longitudes de **CVE-2016-1834**, el desbordamiento
de heap en `xmlStrncat` / `xmlStrncatNew` de libxml2 (`xmlstring.c`), en el
padre 8f30bdf (`pre`), el fix 8fbbf55 (`post`) y master febeb5c (`master`).
Educativo, offline y determinista. Las cadenas son **longitudes** y las
asignaciones **capacidades**: no se construye ninguna cadena de 2 GiB, no
se copia nada y no se ejecuta código de libxml2.

## Dos semánticas, siempre juntas

El desbordamiento de `int` con signo es **comportamiento indefinido** en C.

- `label_c11`: todo camino que necesita un desbordamiento con signo termina
  en `UB_STOP`.
- `label_wrap32`: **variante explícita** que continúa con el wrap en
  complemento a dos y lo marca con `ub = 1`. Es una elección del modelo, no
  una afirmación sobre lo que emite un compilador.

**8fbbf55, observación no verificada:** en el modelo, `post` sigue
calculando `size + len + 1` en `int` (caso `x07`: `UB_STOP` en C11,
`CUR_RET` en wrap32). Es una **observación no verificada** del modelo, no
un hallazgo: no se reprodujo contra un build de libxml2, y master agrega la
comprobación `size > INT_MAX - len`.

## Uso

```sh
sh scripts/build.sh test
python3 scripts/derive_expected.py | diff - fixtures/expected.tsv
sh scripts/build.sh results /tmp/r && diff -r results /tmp/r
```

Ejercicios en [PRACTICE.md](PRACTICE.md); límites en
[LIMITATIONS.md](LIMITATIONS.md); fuentes en [SOURCES.md](SOURCES.md).
Licencia: Apache-2.0.
