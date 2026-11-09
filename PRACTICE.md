# Guía práctica (8 niveles)

Ejercicios **offline y locales** sobre longitudes y tamaños de asignación.
No se construye ninguna cadena ni se copia ningún byte. Cada respuesta está
comprobada en `tests/test_model.c` (`practice_level_N`), compilado con
ASan/UBSan.

Comandos: `sh scripts/build.sh test`; después de `sh scripts/build.sh
results /tmp/r`: `./build/xmlstr case pre|post|master strncat|strncatnew
<len1> <len2> <len>`.

**Básico**

1. **El camino normal.** `cur` de 10 bytes, `add` de 5, `len = 5`. ¿Qué
   tamaño pide `xmlRealloc`? (`practice_level_1`: 16, `OK`)
2. **`xmlStrlen` con `int`.** Con `cur` de exactamente 2^31 bytes, ¿qué
   devuelve el `int len` de la versión pre? (`practice_level_2`: en la
   variante wrap32, INT_MIN; en C11 ya es UB, `ub = 1`)

**Intermedio**

3. **Escritura fuera de límites.** `cur` de 2147483658 bytes y `len =
   INT_MAX`. ¿Cuánto pide la asignación y en qué offset escribe `memcpy`?
   (`practice_level_3`: 10 bytes, offset negativo, `OOB_WRITE` en wrap32)
4. **La misma entrada en C11.** ¿Qué etiqueta da `label_c11`? ¿Por qué no
   se puede decir "desborda" sin elegir una semántica?
   (`practice_level_4`: `UB_STOP`)
5. **El fix 8fbbf55.** Misma entrada en `post`.
   (`practice_level_5`: `NULL_RET` por `if (size < 0)`)

**Avanzado**

6. **Lo que el fix no comprueba.** `cur` de 2147483637 bytes y `len = 10`:
   `size` es positivo pero `size + len + 1` no cabe en `int`. ¿Qué pasa en
   `post`? (`practice_level_6`: wrap32 `CUR_RET` con `ub = 1`; en C11,
   `UB_STOP`. Observación del modelo, **no verificada** contra un binario)
7. **Master.** ¿Cómo lo evita `size > INT_MAX - len` y el cálculo en
   `size_t`? (`practice_level_7`: `OK`, petición de 2^31 bytes, sin UB)
8. **Saturación.** En master, `cur` de 4294967301 bytes: `xmlStrlen`
   devuelve 0. ¿Es seguro? ¿Es correcto? (`practice_level_8`: `LOSSY`,
   resultado de 10 bytes: seguro en memoria, pierde datos)

Más allá: añade un caso a `fixtures/cases.tsv`, regenera
`fixtures/expected.tsv` con `scripts/derive_expected.py` y comprueba que el
modelo en C coincide con la derivación independiente.
