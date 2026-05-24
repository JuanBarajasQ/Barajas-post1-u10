#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h> // API nativa de Windows; necesaria para manejar los contadores de alta resolución

#define REPEAT 40 // Número de pasadas completas sobre el arreglo para promediar la latencia y diluir fluctuaciones

/* * Generador de números pseudoaleatorios utilizando el algoritmo Xorshift.
 * Se prefiere sobre rand() de la librería estándar porque es extremadamente ligero (pocas operaciones de bit).
 * Esto evita añadir retrasos ("overhead") ajenos al acceso físico a la memoria en el bucle principal.
 */
unsigned int fast_rand(unsigned int *state) {
    unsigned int x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* * Función Benchmark de Acceso Aleatorio:
 * Rompe deliberadamente la localidad espacial y el "Hardware Prefetcher" del CPU,
 * forzando saltos impredecibles en memoria para exponer los tiempos de respuesta reales de L1, L2, L3 y RAM.
 */
double bench_rand(size_t n_bytes)
{
    size_t elements = n_bytes; // Cada elemento representa 1 byte (char)
    
    // Asignación dinámica del bloque de memoria principal que se va a testear
    volatile char *arr = (volatile char *)malloc(elements);
    if (!arr) return -1.0; // Control en caso de fallo por falta de memoria
    
    memset((void *)arr, 1, elements); // Inicialización del bloque para asegurar la asignación física de páginas (Demand Paging)

    /* * Creación y permutación de una tabla de índices:
     * Para lograr un acceso puramente aleatorio dentro del bucle sin recalcular índices sobre la marcha,
     * generamos un arreglo secundario con todas las posiciones lineales posibles y luego lo mezclamos.
     */
    size_t *indices = (size_t *)malloc(elements * sizeof(size_t));
    if (!indices) {
        free((void *)arr);
        return -1.0;
    }
    for (size_t i = 0; i < elements; i++) indices[i] = i; // Relleno lineal inicial: 0, 1, 2, ..., N-1

    // Algoritmo Fisher-Yates simplificado para barajar el arreglo de índices de forma uniforme
    unsigned int rand_state = 42; // Semilla estática para garantizar reproducibilidad en las pruebas
    for (size_t i = elements - 1; i > 0; i--) {
        size_t j = fast_rand(&rand_state) % (i + 1);
        size_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }

    // Estructuras de la API de Windows para cronometrar a nivel de micro/nanosegundos
    LARGE_INTEGER frequency, t0, t1;
    QueryPerformanceFrequency(&frequency); // Obtiene los ciclos por segundo del reloj de alta resolución del sistema

    // Variable declarada como 'volatile' para impedir que el compilador descarte u optimice los accesos de lectura
    volatile char dummy = 0;

    // ==================== INICIO DE LA MEDICIÓN CRÍTICA ====================
    QueryPerformanceCounter(&t0); // Captura el conteo de ticks inicial del temporizador

    for (int r = 0; r < REPEAT; r++) {
        for (size_t i = 0; i < elements; i++) {
            /* * El CPU lee el índice desordenado desde el arreglo 'indices' y salta a esa posición en 'arr'.
             * Como el patrón de saltos es aleatorio, el circuito predictor de la CPU (Prefetcher) 
             * falla continuamente, obligando a medir el retardo real del nivel de memoria físico donde reside el dato.
             */
            dummy = arr[indices[i]]; 
        }
    }

    QueryPerformanceCounter(&t1); // Captura el conteo de ticks final del temporizador
    // ===================== FIN DE LA MEDICIÓN CRÍTICA =====================

    // Liberación inmediata de las estructuras dinámicas para evitar fugas de memoria (Memory Leaks)
    free(indices);
    free((void *)arr);

    // Cálculo matemático de la diferencia de ciclos de manera segura ante posibles reordenamientos
    LONGLONG ticks = t1.QuadPart - t0.QuadPart;
    if (ticks < 0) ticks = -ticks; // Filtro preventivo contra anomalías de sincronización de núcleos

    // Conversión matemática de ticks netos a nanosegundos absolutos utilizando la frecuencia del equipo
    double ns = ((double)ticks * 1e9) / (double)frequency.QuadPart;
    
    /* * Inyección marginal de tiempo en caso extremo. 
     * Garantiza formalmente al árbol sintáctico del compilador que la variable 'dummy' 
     * tiene un peso en el resultado, previniendo que remueva el bucle anidado.
     */
    if (dummy == 0) ns += 0.001;

    // Retorna el promedio aritmético final: Nanosegundos totales divididos entre los accesos efectuados
    return ns / (REPEAT * (double)elements);
}


/* Medir tiempo de acceso secuencial a array de N bytes usando QueryPerformanceCounter */
double bench_seq(size_t n_bytes)
{
    volatile char *arr = (volatile char *)malloc(n_bytes);
    if (!arr)
        return -1.0;
        
    memset((void *)arr, 1, n_bytes);
    
    // Variables de alta resolución de Windows
    LARGE_INTEGER frequency;
    LARGE_INTEGER t0, t1;
    
    // Obtener la frecuencia del procesador
    QueryPerformanceFrequency(&frequency);

    // Inicio del reloj
    QueryPerformanceCounter(&t0); 

    for (int r = 0; r < REPEAT; r++) {
        for (size_t i = 0; i < n_bytes; i++) {
            (void)arr[i]; /* acceso de lectura */
        }
    }

    // Fin del reloj
    QueryPerformanceCounter(&t1); 

    // Liberar la memoria del array antes del cálculo para mantener la buena práctica
    free((void *)arr);

    // Calcular nanosegundos en Windows aplicando la frecuencia del sistema
    double ns = (double)(t1.QuadPart - t0.QuadPart) * 1e9 / frequency.QuadPart;
    
    // Retorna la latencia promedio (ns/byte)
    return ns / (REPEAT * (double)n_bytes); 
}

int main(void)
{
    // Tamaños de matriz seleccionados estratégicamente para mapear las capacidades físicas de la caché
    size_t sizes[] = {
        4 * 1024, 8 * 1024, 16 * 1024, 32 * 1024, /* Región L1 (Generalmente <= 32KB por núcleo) */
        64 * 1024, 128 * 1024, 256 * 1024,        /* Región L2 (Típicamente 256KB - 512KB por núcleo) */
        512 * 1024, 1024 * 1024, 4 * 1024 * 1024, /* Región L3 (Compartida en dados del procesador) */
        16 * 1024 * 1024, 64 * 1024 * 1024        /* Región RAM (Supera por completo la memoria estática interna) */
    };
    
    // Cálculo dinámico del número de muestras definidas en el vector anterior
    int n = sizeof(sizes) / sizeof(sizes[0]);
    
    // Cabecera formateada para la salida de la consola de Windows
    printf("%-18s %15s\n", "Array Size (KB)", "ns/byte");
    printf("---------------------------------\n");
    
    // Bucle iterativo de pruebas
    for (int i = 0; i < n; i++)
    {
        // Ejecución secuencial del benchmark para cada tamaño de memoria definido (checkpoint2)
        //double lat = bench_seq(sizes[i]);
        
        // Ejecución aleatoria del benchmark para cada tamaño de memoria definido (checkpoint3)
        double lat = bench_rand(sizes[i]);

        // Impresión formateada traduciendo los bytes a Kilobytes para la lectura de la tabla de resultados
        printf("%-18zu %15.3f\n", sizes[i] / 1024, lat);
    }
    
    return 0;
}