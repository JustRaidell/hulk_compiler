# Compilador HULK — Reporte del Proyecto

## Visión General

Este proyecto implementa un compilador para HULK, un lenguaje de programación con tipado estático y orientado a expresiones, diseñado con fines educativos. El compilador toma un archivo fuente `.hulk` como entrada y produce un ejecutable nativo (`./output`) traduciendo el código HULK a C++ e invocando `g++` como backend. Todo el pipeline — desde el análisis léxico hasta la generación de código — fue implementado desde cero en C++17, evitando deliberadamente herramientas como Flex, Yacc o ANTLR en favor de componentes construidos a mano.

El compilador se invoca como:

```bash
./hulk <archivo.hulk>
```

En caso de éxito produce `./output` en el directorio actual. En caso de error, termina con código `1` (léxico), `2` (sintáctico) o `3` (semántico), imprimiendo un error por línea en `stderr` con el formato `(línea,col) TIPO: mensaje`.

---

## Arquitectura

El compilador está organizado como un pipeline clásico de cuatro fases:

```
Código fuente
    ↓
[Lexer]               — tokenización mediante motor de regex y AFD construidos a mano
    ↓
[Parser]              — parser recursivo descendente, construye el AST
    ↓
[Analizador Semántico] — inferencia de tipos, resolución de nombres, chequeo de tipos
    ↓
[Generador de Código]  — traducción del AST a C++
    ↓
output.cpp  →  g++  →  ./output
```

Cada fase está implementada como un componente C++ autocontenido. El analizador semántico y el generador de código operan sobre el AST usando el patrón Visitor, lo que permite mantener las dos fases completamente separadas sin necesidad de modificar las definiciones de los nodos del AST.

---

## Fase 1 — Lexer

### Motor de Expresiones Regulares

En lugar de depender de la librería estándar o de herramientas como Flex, el lexer está impulsado por un motor de expresiones regulares construido desde los fundamentos. El motor sigue el pipeline clásico:

```
Cadena regex → AST → NFA → AFD (DFA)
```

Cada patrón de token se define como una cadena de expresión regular. El motor parsea la regex en un AST, lo convierte en un Autómata Finito No Determinista (NFA) mediante la construcción de Thompson, y luego lo determiniza en un AFD mediante el algoritmo de construcción de subconjuntos. Cada AFD se usa durante la tokenización para encontrar el prefijo más largo posible en cada posición de la entrada — la estrategia estándar de máximo consumo.

Este enfoque permite definir los patrones de tokens de forma declarativa como cadenas regex, mientras que el matching se realiza eficientemente mediante autómatas deterministas. Implementar este motor desde cero requirió manejar la precedencia de operadores en el parsing de regex (concatenación, alternancia, estrella de Kleene), las transiciones épsilon en la construcción del NFA, y la construcción del conjunto potencia para la determinización — componentes no triviales que habitualmente quedan ocultos detrás de herramientas como Flex.

### Tokenización

El lexer mantiene una tabla de patrones de tokens ordenada por prioridad. En cada posición de la entrada, todos los AFDs compiten y gana el match más largo. Las palabras reservadas tienen prioridad sobre los identificadores por aparecer antes en la tabla.

El manejo de errores cubre dos casos:
- **Caracteres no reconocidos**: cuando ningún AFD encuentra match en la posición actual, el lexer emite un error `LEXICAL` y termina con código `1`.
- **Literales de string inválidos**: cuando un string que comienza con `"` no puede ser reconocido (por contener caracteres inválidos), el lexer reporta un error de literal de string inválido.

---

## Fase 2 — Parser

### De LL(1) a Recursivo Descendente: Una Evolución de Diseño

El parser atravesó una evolución de diseño significativa durante el desarrollo. El plan original era usar el parser tabular LL(1) como parser de producción — una elección natural tras haber invertido un esfuerzo considerable en construirlo. Se produjo una implementación LL(1) completa: se calcularon los conjuntos First y Follow para todos los no terminales, se construyó la tabla de parsing con detección explícita de conflictos, y la gramática fue cuidadosamente factorizada para eliminar la recursión izquierda y la ambigüedad.

Sin embargo, una limitación fundamental se hizo evidente: un parser tabular LL(1) produce una secuencia plana de pasos de derivación, no un árbol. Para construir un AST a partir de esas derivaciones, habría que adjuntar acciones semánticas a cada regla de producción — implementar esencialmente gramáticas atributadas. Esto habría requerido propagar atributos sintetizados y heredados a través de la tabla de parsing, un mecanismo considerablemente más complejo de implementar y depurar de lo que parece a primera vista, y que acopla estrechamente las reglas de gramática con la lógica de construcción del AST.

Se tomó la decisión de retirar el parser LL(1) del pipeline de producción y reemplazarlo por un parser recursivo descendente escrito a mano. El descenso recursivo se mapea de forma natural a la construcción del AST: cada método de parsing devuelve un nodo, y el árbol emerge orgánicamente de la estructura de llamadas. El esfuerzo invertido en la implementación LL(1) no fue en vano — el trabajo de factorización de gramática que requirió se trasladó directamente, y el validador permanece en el código como herramienta para detectar conflictos cuando se añaden nuevas producciones.

Esta evolución refleja una verdad más amplia sobre la construcción de compiladores: la solución teóricamente elegante no siempre es la más práctica. El parser LL(1) es un artefacto formal hermoso; el parser recursivo descendente es lo que realmente se entrega.

### Parser Recursivo Descendente

El parser de producción es un parser recursivo descendente escrito a mano. Cada no terminal de la gramática HULK corresponde a un método `parse*`, y la jerarquía de precedencia de la gramática queda codificada directamente en la estructura de llamadas — los operadores de menor precedencia se parsean más arriba en la cadena de llamadas, los de mayor precedencia más abajo.

Decisiones de diseño clave:

**Parsing orientado a expresiones**: en HULK, todo es una expresión. Las construcciones `let`, `if`, `while`, `for` y los bloques producen valores y se parsean como expresiones. Esto se refleja en que `parseExpr()` es el punto de entrada universal, con todas las construcciones devolviendo `unique_ptr<ASTNode>`.

**Precedencia de operadores**: la cadena de precedencia completa, de menor a mayor, está codificada como una secuencia de métodos mutuamente recursivos:

```
parseExpr         →  :=  (asociativo por la derecha)
parseCheckExpr    →  is
parseOrExpr       →  |
parseAndExpr      →  &
parseEqualityExpr →  == !=
parseCompareExpr  →  > < >= <=
parseConcatExpr   →  @ @@
parseArithExpr    →  + -
parseTerm         →  * /
parsePot          →  ^ %  (asociativo por la derecha mediante recursión)
parseUnaryExpr    →  - !
parseFactor       →  átomos y sufijos
```

**Encadenamiento de sufijos**: el acceso a miembros (`.`), las llamadas a función (`()`) y la indexación (`[]`) se manejan en un bucle unificado dentro de `parseFactor`, permitiendo encadenamientos arbitrarios: `obj.method(arg)[0].field`.

**Asociatividad por la derecha**: los operadores `^` y `%` usan recursión en vez de iteración en `parsePot`, implementando correctamente la asociatividad por la derecha: `2^3^2` se parsea como `2^(3^2)`.

**Desambiguación de literales de vector**: dentro de `[...]`, el parser lee hasta el nivel `|` de la jerarquía de expresiones para evitar consumir el separador `|` antes de determinar si la construcción es un literal de vector o una expresión generadora.

### AST

El parser produce un AST tipado con 21 tipos de nodos, cubriendo todas las construcciones de HULK. Cada nodo hereda de `ASTNode`, que contiene un campo `inferred_type` poblado por el analizador semántico para uso durante la generación de código. Cada nodo implementa `accept(Visitor&)` para soportar el patrón Visitor usado en las fases posteriores.

---

## Fase 3 — Análisis Semántico

El analizador semántico está implementado como un `Visitor` sobre el AST. Realiza tres tareas entrelazadas: resolución de nombres, chequeo de tipos e inferencia de tipos.

### Tabla de Símbolos

La tabla de símbolos gestiona tres espacios de nombres: variables (pila de scopes), funciones (mapa global plano) y tipos (mapa global plano). Los scopes de variables se apilan y desapilan a medida que el analizador entra y sale de bindings `let`, cuerpos de función, bucles `for` y definiciones de tipos. Las funciones y los tipos viven en un scope global porque HULK no soporta declaraciones de funciones anidadas.

La tabla se pre-puebla con tipos built-in (`Number`, `Boolean`, `String`, `Object`) y funciones built-in (`print`, `sqrt`, `sin`, `cos`, `exp`, `log`, `rand`, `range`), así como los protocolos built-in `Iterable` y `Vector`.

### Análisis en Dos Pasadas

A nivel de programa, el analizador realiza dos pasadas sobre las declaraciones de primer nivel:

1. **Pasada de registro**: todas las funciones, tipos y protocolos se registran en la tabla de símbolos antes de analizar ningún cuerpo. Esto permite funciones mutuamente recursivas y referencias hacia adelante entre tipos.
2. **Pasada de análisis**: cada cuerpo de declaración se analiza con la tabla de símbolos completa disponible.

### Sistema de Tipos

El sistema de tipos de HULK combina tipado nominal (para tipos regulares) con tipado estructural (para protocolos). Los componentes clave son:

**`lca(a, b)`**: calcula el Ancestro Común Más Bajo de dos tipos en la jerarquía de herencia. Se usa para determinar el tipo de las expresiones `if`, donde todas las ramas deben ser compatibles y el tipo resultado es su LCA.

**`satisfies(tipo, protocolo)`**: verifica si un tipo satisface estructuralmente un protocolo comprobando que todos los métodos requeridos están presentes. Implementa la conformancia implícita de protocolos de HULK — un tipo no necesita declarar explícitamente que implementa un protocolo.

**`isCompatible(actual, esperado)`**: la verificación de compatibilidad unificada usada en todo el analizador. Maneja tres casos: identidad, satisfacción estructural de protocolo y subtipado nominal mediante `lca`. Esta función es el único punto de entrada para todas las verificaciones de compatibilidad de tipos, incluyendo asignación, argumentos de función y tipos de retorno.

**Inferencia de tipos**: donde las anotaciones de tipo están ausentes, el analizador infiere los tipos de abajo hacia arriba a partir de las expresiones. El tipo inferido de cada nodo se almacena en `node.inferred_type` para uso del generador de código.

### Reporte de Errores

El analizador recoge todos los errores semánticos sin abortar, permitiendo reportar múltiples errores en una sola ejecución. Los errores se imprimen en `stderr` en el formato requerido y el proceso termina con código `3`. Las clases de errores detectados incluyen: variables y funciones no declaradas, incompatibilidades de tipos, targets de reasignación inválidos, herencia circular, declaraciones duplicadas, violaciones de protocolo e instanciación de protocolos abstractos.

---

## Fase 4 — Generación de Código

El generador de código está implementado como un segundo `Visitor` sobre el AST — ahora anotado con los tipos inferidos por el analizador semántico. Produce un único archivo fuente C++ (`output.cpp`) que luego es compilado por `g++` para producir el ejecutable final.

### Mapeo de Tipos

Los tipos de HULK se mapean a C++ de la siguiente manera:

| HULK | C++ |
|------|-----|
| `Number` | `double` |
| `Boolean` | `bool` |
| `String` | `std::string` |
| `Object` | `HulkObject*` |
| `T[]` | `std::vector<T>` |
| `T*` | `std::vector<T>` |
| Tipos definidos por el usuario | Puntero a clase C++ |

### Traducción de Expresiones a Sentencias

Dado que HULK está orientado a expresiones y C++ está orientado a sentencias, construcciones como `if`, `while`, `for`, `let` y las expresiones de bloque se traducen a lambdas C++ inmediatamente invocadas:

```cpp
// HULK: let x = 1 in x + 1
[&]() -> double {
    double x = 1;
    return x + 1;
}()
```

Esto preserva la semántica de retorno de valor de todas las construcciones de HULK sin necesitar una IR separada ni un mecanismo de paso de valores.

### Jerarquía de Tipos

Los tipos definidos por el usuario se emiten como `struct`s de C++ que heredan de `HulkObject` (o de su padre declarado). Todos los métodos se marcan como `virtual` para soportar polimorfismo. Los protocolos se emiten como structs abstractos de C++ con métodos puramente virtuales. Se genera un método `toString()` para cada tipo definido por el usuario para soportar la conversión a string usada por `@` y `@@`.

### Soporte en Tiempo de Ejecución

Un header `hulk_runtime.h` se emite al principio de cada archivo generado. Define la clase base `HulkObject`, alias de tipo para los tipos primitivos, e implementaciones de todas las funciones built-in (`hulk_print`, `hulk_sqrt`, `hulk_concat`, `hulk_range`, etc.).

---

## Limitaciones Conocidas

**Funciones lambda**: las expresiones de función anónimas (`function(x) => x + 1`) son parseadas y representadas en el AST pero no están completamente soportadas en las fases de análisis semántico y generación de código.

**Sintaxis de vectores**: el compilador implementa la sintaxis de vectores tal como está definida en la especificación del lenguaje HULK (`[1, 2, 3]` y `[x^2 | x in range(1,10)]`). La sintaxis alternativa usada en algunos casos de prueba (`new Number[5]`, `{10, 20, 30}`) no forma parte de la especificación oficial y por tanto no está soportada.

**Posiciones en los errores**: los mensajes de error reportan actualmente la posición `(0,0)` para todos los errores. El seguimiento de línea y columna es una mejora planificada que no fue implementada dentro del plazo del proyecto.

**Tipos functor**: la sintaxis de anotación de tipo `(T) -> T'` para funciones de orden superior es parseada y almacenada, pero el chequeo de tipos y la generación de código para functores está solo parcialmente implementado.

---

## Reflexiones de Diseño

La decisión de construir el motor de regex, el validador LL(1) y el parser recursivo descendente completamente a mano — en lugar de usar Flex, Yacc o ANTLR — fue deliberada. Estas herramientas abstraen las partes más instructivas de la construcción de compiladores. Implementar desde cero la construcción de Thompson, la construcción de subconjuntos, el cálculo de conjuntos First/Follow y la construcción de la tabla LL(1) produjo una comprensión más profunda de cómo funcionan realmente los lexers y los parsers, a costa de un tiempo de implementación considerablemente mayor.

La transición de LL(1) a recursivo descendente fue quizás el momento más instructivo del proyecto. Ilustró que lo formal y lo práctico no siempre coinciden: el parser LL(1) es teóricamente sólido y produce un parse correcto, pero integrar la construcción del AST en un framework tabular requiere gramáticas atributadas — una capa de complejidad que el descenso recursivo absorbe de forma natural. Reconocer esa limitación a mitad del proyecto, en lugar de forzar una solución que habría sido más difícil de mantener, fue la decisión correcta.

El patrón Visitor resultó ser la elección arquitectónica correcta para la segunda mitad del pipeline. Tener el analizador semántico y el generador de código como implementaciones separadas de Visitor sobre un AST compartido facilitó mantener las responsabilidades separadas, añadir nuevos pases de análisis y razonar sobre cada fase de forma independiente. La alternativa — incrustar métodos `analyze()` y `generateCode()` directamente en cada nodo del AST — habría acoplado las tres responsabilidades en un único archivo y hecho el desarrollo incremental considerablemente más difícil.

Elegir C++ como backend de compilación fue una decisión pragmática que resultó acertada. El código generado es legible, `g++` maneja automáticamente el layout de memoria y los detalles del ABI de la plataforma, y los errores que escapan al análisis semántico son atrapados en el paso de compilación de C++ en lugar de producir comportamiento incorrecto silencioso.

Por último, merece mención la divergencia entre la especificación del lenguaje y la suite de tests. Varios casos de prueba utilizan sintaxis que no aparece en la especificación oficial de HULK — en particular la sintaxis de inicialización de arrays. Este es un fenómeno del mundo real: las especificaciones y las implementaciones divergen, y el autor de un compilador debe decidir cuál seguir. Este compilador sigue la especificación. Donde existen divergencias, quedan documentadas aquí para que los evaluadores tengan el panorama completo.

---

*Este compilador fue construido de forma incremental, una fase a la vez, validando cada componente antes de comenzar el siguiente. El resultado es un pipeline que, si bien no está completo en todos los casos extremos, es coherente, bien estructurado y honesto sobre lo que soporta y lo que no.*
