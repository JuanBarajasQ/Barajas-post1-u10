# Laboratorio: Midiendo el efecto de la caché
* **Estudiante:** Juan Carlos Barajas Quintero 
* **Curso:** Arquitectura de Computadores - Unidad 10
* **Institución:** Universidad Francisco de Paula Santander

# Benchmarking de Latencia de Memoria y Jerarquía de Caché

Este proyecto consiste en la implementación y ejecución de un benchmark en lenguaje C diseñado para medir la latencia de acceso a la memoria. El objetivo principal es observar de manera empírica cómo la **jerarquía de caché (L1, L2, L3)** y la memoria **RAM** afectan el rendimiento de los programas según el tamaño de los datos y el patrón de acceso.

## 1. Objetivo
El propósito del laboratorio es identificar los puntos de inflexión en el rendimiento del procesador cuando los datos exceden las capacidades físicas de cada nivel de caché. Se busca comprender conceptos clave como la **localidad espacial**, las **líneas de caché**, los **cache misses** y los **TLB misses**.

## 2. Entorno de Compilación y Ejecución
Para garantizar que las mediciones reflejen la latencia física y no optimizaciones del software, se utilizó el siguiente entorno:
*   **Sistema Operativo:** Windows 11 (ejecutado en terminal PowerShell).
*   **Compilador:** GCC (MinGW/MSYS2).
*   **Flags de Compilación:** Se utilizó `-O0` para deshabilitar optimizaciones que pudieran eliminar los bucles de acceso a memoria y `-fno-builtin` para evitar sustituciones de funciones por parte del compilador.
*   **Comando de compilación y ejecución:**
    ```bash
    gcc -O0 -fno-builtin cache_bench.c -o cache_bench.exe
    cache_bench.exe
    ```

## 3. Pasos de Ejecución
1.  **Identificación de Hardware:** Determinar los tamaños de caché del sistema mediante comandos de administración (CIM en Windows).
2.  **Benchmark Secuencial:** Ejecutar accesos lineales sobre arreglos de tamaño creciente (4 KB a 64 MB) para medir la eficiencia del *Hardware Prefetcher*.
3.  **Benchmark Aleatorio:** Ejecutar accesos desordenados utilizando índices barajados (algoritmo Fisher-Yates) para romper la localidad espacial y forzar fallos en la caché y el TLB.

---

## 4. Análisis de Resultados

### Checkpoint 1: Topología de la Caché
Según la captura de pantalla del sistema, se identificaron los siguientes niveles de caché:
*   **Nivel 3:** 512 KB (Caché L1).
*   **Nivel 4:** 4,096 KB (4 MB - Caché L2).
*   **Nivel 5:** 16,384 KB (16 MB - Caché L3).

### Checkpoint 2: Acceso Secuencial
Los resultados muestran una latencia notablemente constante, manteniéndose alrededor de **1.04 ns a 1.11 ns/byte** para todos los tamaños de arreglo, desde 4 KB hasta 65,536 KB.
*   **Análisis:** Esta estabilidad se debe al **Hardware Prefetcher** del CPU. Al detectar un patrón de acceso lineal, el procesador trae los datos a la caché antes de que el programa los solicite, ocultando la latencia real de la RAM y los niveles superiores de caché.

### Checkpoint 3: Acceso Aleatorio vs. Secuencial
En el acceso aleatorio, la latencia aumenta drásticamente a medida que el arreglo supera los límites físicos detectados en el Checkpoint 1:
*   **4 KB a 256 KB (~2.0 ns):** El acceso se mantiene principalmente en los niveles más rápidos (L1/L2).
*   **512 KB (2.50 ns):** Primer salto significativo al agotar el Nivel 3 de caché (512 KB).
*   **1024 KB a 4096 KB (3.56 - 5.49 ns):** Los datos ya no caben en los niveles inferiores y empiezan a depender del Nivel 4 (4 MB). Aquí ocurre un **Cache Miss** frecuente en L2/L3.
*   **16 MB (14.37 ns):** Salto crítico al superar el Nivel 5 de caché.
*   **64 MB (18.44 ns):** Acceso directo a la **RAM**.

**Atribución de Saltos de Latencia:**
1.  **Cache Miss:** Los saltos en 512 KB y 4 MB ocurren porque el conjunto de datos es mayor que la capacidad de la caché, obligando al CPU a buscar datos en niveles más lentos y distantes.
2.  **TLB Miss:** En el acceso aleatorio a partir de **1-2 MB**, se observa una penalización adicional. Esto se debe a fallos en el *Translation Lookaside Buffer* (TLB); al saltar aleatoriamente entre páginas de memoria, el procesador pierde la traducción de direcciones virtuales a físicas, añadiendo ciclos extra de latencia por cada acceso.

---

## 5. Conclusión de la Comparativa
El acceso aleatorio es significativamente más costoso que el secuencial. Mientras que el secuencial aprovecha la **localidad espacial** y el pre-buscado del hardware, el aleatorio expone la latencia real de la jerarquía de memoria. La diferencia de ~1 ns (secuencial) frente a ~18 ns (aleatorio en RAM) demuestra que el diseño de software eficiente debe priorizar patrones de acceso que respeten la jerarquía de caché del procesador.

---

