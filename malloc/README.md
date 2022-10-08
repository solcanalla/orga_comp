# TP2 95.57: Malloc

En este documento se listarán todos los detalles de implementación relacionados con los bloques y algorítmos que procesan a los mismos dentro del heap. Esta implementación funcionará en x86 de 64 bits, los punteros serán de 8 bytes y cualquier puntero que se devuelva debe estar alineado a 16 bytes.

## 1. Diseño

### 1.1 Diseño del bloque
	
Decidimos implementar el trabajo con ***listas implícitas***. El algorítmo utilizado para la búsqueda de bloques libres es ***first fit***. El mismo tendrá la siguiente estructura:

```
  BLOQUE:

+------------------------------------+
| HEADER:  Block size  |  0  0  0  A | <-- NORMAL POINTER
+------------------------------------+
|                                    | <-- BLOCK  POINTER
|                                    |
|                                    |
|                                    |
|              PAYLOAD               |
|                                    |
|                                    |
|                                    |
|                                    |
+------------------------------------+
|                                    |
|         PADDING (OPCIONAL)         |
|                                    |
+------------------------------------+
| FOOTER:  Block size  |  0  0  0  A |
+------------------------------------+

```

### 1.2 Tamaño del bloque 

Siendo esta una implementación donde cada puntero tiene 8 bytes y donde cada bloque debe estar alineado a 16 bytes, todo bloque no vacío tendrá de tamaño mínimo 32 bytes. Si el bloque está ocupado, tendría los 8 bytes del header, más los 8 del footer, y como el mínimo dato que se puede guardar en el bloque es de 1 byte, el padding deberá ser de 15 bytes (ya que sin este serían 17 bytes y debemos llevar el tamaño al múltiplo de 16 más cercano), dando así un tamaño mínimo de bloque de 32 bytes. De lo contrario, si el bloque está vacío simplemente tendrá el header, el footer y 16 bytes de padding, dejando así 32 bytes en total.

### 1.3 Epílogo y prólogo 

Nuestra implementación, en caso de querer implementar coalescing, deberá tener un bloque de prólogo y otro de epílogo.

```
Bloque vacío (padding)                               
 v                                                   
+---------------------------------------------------+
|X|Prólogo|               ...               |Epílogo|
+---------------------------------------------------+
           ^ Inicio Heap
```

El prólogo consistirá de un bloque de 16 bytes vacío, que solamente tendrá el header y el footer. El epílogo será un bloque que solo tendrá el header. Ambos son útiles pues siempre estarán marcados como bloques ocupados y por lo tanto no pueden ser liberados por el coalescing.

Además, solamente tendremos una referencia estática al inicio del heap, que es un puntero asociado al primer byte posterior al footer del prólogo. Será útil para iterar por el heap hasta el epílogo (facilmente identificable pues es el único bloque de tamaño cero).

### 1.4 Primitivas y estructuración de datos  

Actualmente muchas de estas primitivas están como macros en el código. Para la explicación de las mismas se las enunciará como funciones estáticas. Además notamos que el puntero ``` p ``` siempre estará asignado a la dirección del header del bloque; en cambio el puntero ```bp (block pointer)``` estará asignado a la próxima dirección del header, es decir, a los 8 bytes posteriores donde inicia el payload del mismo.

```
/*
 * Tomamos la dirección del bloque, se castea en un entero y con la máscara que se le aplica
 * se eliminan los cuatro bits que deberían ser ceros para que el tamaño sea el correcto.
 * Precondición: el puntero debe ser diferente de NULL.
 * Postcondición: devuelve el tamaño del bloque. 
 */
static int get_size(void* p){
  return (*(unsigned int*)(p) & ~0xF);
}

/*
 * Se recibe el puntero al header del bloque, y se devuelve un entero en función de si el bloque
 * está ocupado o no.
 * Precondición: el puntero debe ser diferente de NULL.
 * Postcondición: se devuelve 0 si el bloque está libre, 1 de lo contrario.
 */
static int is_free(void* p){
  return (p & 0x1);
}
```

```
/*
 * Recibo un block pointer, calculo su tamaño y me muevo tantos bytes como
 * este indique hacia la posición del siguiente bloque.
 * Precondición: el puntero debe ser diferente de NULL.
 * Postcondición: devuelve el puntero al próximo bloque.
 */
static void* next_blkp(void* bp){
	int shift = get_size(bp - 8); // Con los 8 desplazamos la referencia al header.
	return (p + shift);
}

/*
 * Recibo un block pointer, calculo el tamaño del bloque anterior, indicado
 * por el footer vecino, y me muevo tantos bytes como este indique hacia la posición del 
 * bloque anterior.
 * Precondición: el puntero debe ser diferente de NULL.
 * Postcondición: devuelve el puntero al bloque anterior.
 */
static void* prev_blkp(void* bp){
	int shift = get_size(bp - 16); // Con los 16 desplazamos la referencia al footer del bloque previo.
	return (bp - shift);
}
```

### 1.5 Primeras llamadas a mem_sbrk(int)

Dentro de la función ```mm_init(void)``` llamamos a ```mem_sbrk(32)``` para reservar 32 bytes del heap. En los primeros 8 ponemos padding, en los próximos 16 bytes el header y footer del prólogo y en los 8 bytes restantes el header del epílogo. Dejamos el puntero al inicio del heap con offset de 24 bytes desde el inicio. Luego dentro del ```extend_heap(int words)``` llamamos a mem_sbrk y reservamos ```size = size + 8``` (si es par, de lo contrario: ``` (size+1)*8```) bytes para crear un bloque libre y ajustamos el epílogo luego de este. El block pointer del primer bloque libre estará a 24 bytes (finalización del footer del epílogo) + 8 bytes por el header, dejándolo así alineado a 16 bytes.

## 2. Heap Checker

### 2.1 Potenciales inconsistencias entre el modelo y la realidad

A continuación se describen una serie de problemas que pueden surgir dentro del heap y posibles
formas de detectar cada uno:

#### 2.1.1 El tamaño del bloque que contiene el header y footer no coinciden

```
/*
 * Dado un block pointer diferente de NULL y perteneciente al heap válido, se devuelve 1 si el 
 * header y footer coinciden, 0 de lo contrario.
 */
static int header_and_footer_coinciden(void* bp){
  return (*(unsigned int*)(GET_HEADER(bp)) == *(unsigned int*)(GET_FOOTER(bp)));
}
```


#### 2.1.2 Puede aparecer algún bloque no alineado a 16 bytes

```
/*
 * Recibe un puntero a bloque y devuelve 1 si está alineado el mismo, 0 de lo contrario.
 * Precondición: el puntero debe ser diferente de NULL y estar contenido de forma válida en el heap.
 * Postcondición: se devuelve 1 o 0 en función de si el bloque está correctamente alineado a 16
 * bytes o no, correspondientemente.
 */
int is_aligned(void* p){
  return ((*(unsigned int*)(p+8) % 16)? 0 : 1);
}

/*
 * Dado un puntero al comienzo del heap diferente de NULL, se devuelve 1 si todos los 
 * punteros del mismo están alineados a 16 bytes, 0 de lo contrario.
 */							
static int heap_is_aligned(void* p){
  /*Es necesario sumarle 8 pues ptr no es un block pointer*/-------------v
  for (void* ptr = inicio_heap; ptr != final_heap; ptr = next_blkp(ptr + 8)){
      if (!is_aligned(ptr)) return 0;
    }
  return 1;
}
```
#### 2.1.3 Alguna dirección de memoria de la lista puede no ser válida

```
/*
 * Se recibe un puntero y se verifica si este está contenido dentro del heap.
 * Precondiciones: el puntero es diferente de NULL.
 * Postcondiciones: Si la dirección del bloque es menor al inicio o mayor al final del heap, 
 * se devuelve false, true de lo contrario.
 */
static bool esta_dentro_del_heap(void* p){
  if ((*(uint64_t*)mem_start_brk > *(uint64_t*)p) ||
      (*(uint64_t*)mem_brk < *(uint64_t)p){
      return false;    
    }
  return true;
}

```
