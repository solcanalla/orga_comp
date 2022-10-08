#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// Por favor completar.
team_t team = {
    "g07",  // Número de grupo,
    "Jonathan David Rosenblatt",     // Nombre integrante 1
    "jrosenblatt@fi.uba.ar",         // E-mail integrante 1
    "Sol Orive",                     // Nombre integrante 2
    "morive@fi.uba.ar"               // E-mail integrante 2
};

#define WSIZE 8
#define DSIZE 16 
#define TAM_MINIMO 2*DSIZE
#define CHUNKSIZE (1<<12)
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define OP_OR(size,alloc) ((size)|(alloc))
#define CREAR_HEADER(size,alloc) OP_OR(size,alloc)
#define CREAR_FOOTER(size,alloc) OP_OR(size,alloc)
#define GET_VALOR(p) (*(unsigned int*)(p))
#define ASIGNAR(p,val) (*(unsigned int*)(p) = (val))
#define GET_SIZE(p) (GET_VALOR(p) & ~0xF)
#define IS_FREE(p) !(GET_VALOR(p) & 1)
#define GET_HEADER(bp) ((char*)(bp) - WSIZE)
#define GET_FOOTER(bp) ((char*)(bp) + GET_SIZE(GET_HEADER(bp)) - DSIZE)
#define GET_BLKP(p) (p+WSIZE)
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE((char*)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))
#define IS_ALIGNED(bp) !((long)(bp)%DSIZE)
#define HEADER_Y_FOOTER_COINCIDEN(bp) (GET_VALOR(GET_HEADER(bp)) == GET_VALOR(GET_FOOTER(bp)))

#define RESET "\x1b[0m"
#define GREEN "\x1b[1m\x1b[32m"
#define RED "\x1b[1m\x1b[31m"
#define AZUL "\x1b[1m\x1b[34m"

static void* inicio_heap;

/* FUNCIONES AUXILIARES */

/*
-> Implementar estas funciones restantes, particularmente las últimas dos para poder armar el malloc y realloc.
-> En caso de querer usar otro algorítmo, diferente del first_fit, se pueden cambiar con macros.
 */

/*
 * Recibe un puntero al header de un bloque e imprime la información del mismo en función de su estado actual.
 */
void print_block(void* p){

	// A veces es preferible usar el block pointer.
	void* bp = GET_BLKP(p);

	char* isfree    = (IS_FREE(p)) ? "FREE": "USED";
	char* isaligned = (IS_ALIGNED(bp)) ? GREEN"ALIGNED"RESET : RED"NOT ALIGNED"RESET ;
	char* good_hf   = (HEADER_Y_FOOTER_COINCIDEN(bp)) ? GREEN"ghf"RESET : RED"bhf"RESET; // Verificamos que el header y el footer sean iguales.
	printf("[%s | %s | OFFSET = %ld \t| %d Bytes \t | %s]\n",good_hf,isfree,p-inicio_heap,GET_SIZE(p),isaligned);
}

/*
 * Esta función imprime todos los bloques del heap con información importante del mismo
 */
void print_heap(){
	// Recordemos que primer_libre y ultimo libre son block pointers.

	printf(AZUL"\n +------------------------ PRINT HEAP ---------------------------+\n"RESET);
	for (void* p = inicio_heap; GET_SIZE(p) != 0 ; p = GET_HEADER(NEXT_BLKP(GET_BLKP(p)))) print_block(p);
	printf(AZUL" +------------------------ PRINT HEAP ---------------------------+\n\n"RESET);
}

/*
 * Recibo una línea, si ocurre que algún bloque de la lista de libres no está libre, que
 * algún bloque esté mal alineado o que no tengan el mismo header y footer, se imprimirá
 * un mensaje de error por stderr, indicando también la línea desde la que se llamó
 * De lo contrario, es decir, si no detecta errores, no imprime nada.
 */
void mm_checkheap(int line){

	int hubo_error = 0;
	for (void* p = inicio_heap; GET_SIZE(p) != 0 ; p = GET_HEADER(NEXT_BLKP(GET_BLKP(p)))){

		//printf(AZUL"CON TAMAÑO: %d\n",GET_SIZE(p));
		
		void* bp = GET_BLKP(p);
		int isaligned = IS_ALIGNED(bp);
		int good_hf   = HEADER_Y_FOOTER_COINCIDEN(bp);
		if (!isaligned || !good_hf){
			hubo_error = 1;
			if (!isaligned) fprintf(stderr,RED"ERROR, El bloque no está alineado.\n"RESET);
			else		 	fprintf(stderr,RED"ERROR, El header y footer no coinciden.\n"RESET);
		}
	}
	if (hubo_error) fprintf(stderr,RED"Line: %d\n"RESET,line);
}

void* first_fit(size_t size){

	// Continuamos hasta llegar al epílogo de tamaño cero
	for (void* p = inicio_heap; GET_SIZE(p) > 0; p = GET_HEADER(NEXT_BLKP(GET_BLKP(p)))){
		if (IS_FREE(p) && size <= GET_SIZE(p)) return (GET_BLKP(p)); // Para devolver el block pointer.
	}

	return NULL;
}

void ubicar_en_heap(void* bp, size_t size){

	size_t old_size = GET_SIZE(GET_HEADER(bp));
	size_t remaining = old_size - size;
	if (remaining < TAM_MINIMO){ // En este caso no necesitamos partir el bloque.
		ASIGNAR(GET_HEADER(bp),CREAR_HEADER(old_size,1));
		ASIGNAR(GET_FOOTER(bp),CREAR_FOOTER(old_size,1));
	} else {
		ASIGNAR(GET_HEADER(bp),CREAR_HEADER(size,1));
		ASIGNAR(GET_FOOTER(bp),CREAR_FOOTER(size,1));
		bp = NEXT_BLKP(bp);
		ASIGNAR(GET_HEADER(bp),CREAR_HEADER(remaining,0));
		ASIGNAR(GET_FOOTER(bp),CREAR_FOOTER(remaining,0));
	}
}

void* coalesce(void* bp){

	size_t size = GET_SIZE(GET_HEADER(bp));
	size_t esta_ocupado_el_anterior = !IS_FREE(GET_FOOTER(PREV_BLKP(bp)));
	size_t esta_ocupado_el_proximo  = !IS_FREE(GET_HEADER(NEXT_BLKP(bp)));

	/*
	 * En este caso liberamos un bloque rodeado de vecinos ocupados:
	 */
	if (esta_ocupado_el_anterior && esta_ocupado_el_proximo){
		return bp;
	}

	/* CASO 2 */
	// Acabamos de liberar un bloque cuyo vecino próximo también
	// está libre => merge.
	if (esta_ocupado_el_anterior && !esta_ocupado_el_proximo){
		size += GET_SIZE(GET_HEADER(NEXT_BLKP(bp)));  // Vamos a sobreescribir
		ASIGNAR(GET_HEADER(bp),CREAR_HEADER(size,0)); // el próximo bloque vacío.
		ASIGNAR(GET_FOOTER(bp),CREAR_FOOTER(size,0));
	}

	/* CASO 3 */
	// Acabamos de liberar un bloque cuyo vecino anterior también
	// está libre => merge.
	else if (!esta_ocupado_el_anterior && esta_ocupado_el_proximo){
		size += GET_SIZE(GET_HEADER(PREV_BLKP(bp)));
		ASIGNAR(GET_HEADER(PREV_BLKP(bp)),CREAR_HEADER(size,0));
		ASIGNAR(GET_FOOTER(bp),CREAR_FOOTER(size,0));
		bp = PREV_BLKP(bp);
	}

	/* CASO 4 */
	// Acabamos de liberar un bloque rodeado de bloques libres
	// =>merge.
	else{
		size += GET_SIZE(GET_HEADER(NEXT_BLKP(bp))) +  GET_SIZE(GET_HEADER(PREV_BLKP(bp)));
		ASIGNAR(GET_HEADER(PREV_BLKP(bp)),CREAR_HEADER(size,0));
		ASIGNAR(GET_FOOTER(NEXT_BLKP(bp)),CREAR_FOOTER(size,0));
		bp = PREV_BLKP(bp);
	}

	// Ahora al nuevo bloque libre le introducimos los punteros correspondientes de la lista.
	return bp;
}

void* extend_heap(size_t words){

	void* bp;
	size_t size = (words % 2) ? (words+1) * 8 : words * 8;
	if (size < TAM_MINIMO) size = TAM_MINIMO;
	

	if ((long)(bp = mem_sbrk(size)) == -1){ // mem_sbrk nos devuelve el puntero al inicio
		return NULL;                        // del espacio allocado en el heap!
	}
	ASIGNAR(GET_HEADER(bp),CREAR_HEADER(size,0)); // Inicializo el nuevo bloque con la memoria dada con el
	ASIGNAR(GET_FOOTER(bp),CREAR_FOOTER(size,0)); // último bit en cero sobreescribiento el epílogo anterior
	ASIGNAR(GET_HEADER(NEXT_BLKP(bp)),CREAR_HEADER(0,1));

	// El puntero que devuelve mem_sbrk está al finalizar el epílogo => hay que pisarlo
	// y sobreescribirlo.

	return coalesce(bp); // El coalesce ajustará los punteros internos del bloque libre.
}


//======================================================================================================

/* FUNCIONES OBLIGATORIAS */

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {

	if ((inicio_heap = mem_sbrk(32)) == NULL){
		return -1;
	}
	ASIGNAR(inicio_heap,0);     // Estos 8 bytes necesitamos ponerlos para alinear.
	ASIGNAR(inicio_heap +  8, CREAR_HEADER(16,1));  //OP_OR(16,1) = 16 | 1 = 0x10 | 0x1 = 0x11
	ASIGNAR(inicio_heap + 16, CREAR_FOOTER(16,1));
	ASIGNAR(inicio_heap + 24, CREAR_HEADER(0,1));

	inicio_heap += 24;

	if (extend_heap(CHUNKSIZE/8) == NULL){
		return -1;
	}

	mm_checkheap(__LINE__);
    return 0;
}


/*
 * mm_malloc - Allocate a block.
 */
void *mm_malloc(size_t size) {

	if (size <= 0){
		fprintf(stderr,"Can't allocate negative or null amount of bytes.");
		return NULL;
	}

	size_t block_size;
	void* bp;

	if (size <= 16) block_size = TAM_MINIMO; // Si se pide menos del tamaño mínimo (sin header ni footer) lo ajustamos.
	else block_size = DSIZE * ((size + TAM_MINIMO - 1)/DSIZE); // Para entender esta línea, se puede comparar los gráficos de y = x contra y = 16*floor(floor(x+31)/16) => alinea todo a 16 bytes, teniendo en cuenta el padding.
	mm_checkheap(__LINE__);

	// Buscamos dentro de la lista de bloques libres el primero que contenga el espacio suficiente.
	if ((bp = first_fit(block_size)) != NULL){
		ubicar_en_heap(bp,block_size);
		mm_checkheap(__LINE__);
		return bp;
	}


	int a_expandir = (block_size > CHUNKSIZE)? block_size/8 : CHUNKSIZE/8 ;
	if ((bp = extend_heap(a_expandir)) == NULL){
		return NULL;
	}

	ubicar_en_heap(bp,block_size);
	mm_checkheap(__LINE__);
	return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {

  	// El coalesce se encarga de actualizar los punteros internos en
  	// caso de que fuese necesario.

	ASIGNAR(GET_HEADER(ptr),CREAR_HEADER(GET_SIZE(GET_HEADER(ptr)),0));
	ASIGNAR(GET_FOOTER(ptr),CREAR_FOOTER(GET_SIZE(GET_HEADER(ptr)),0));
	mm_checkheap(__LINE__);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {

	// PTR ES UN BLOCK POINTER!
	if (size <= 0 || ptr == NULL){
		fprintf(stderr,"Can't allocate negative or null amount of bytes and pointer can't be NULL.");
		return NULL;
	}

	size_t old_size = GET_SIZE(GET_HEADER(ptr));
	size_t block_size = DSIZE * ((size + TAM_MINIMO - 1)/DSIZE);
	void* new_bp = NULL;

	if (old_size >= block_size){
		return ptr;
	}
	
	if ((new_bp = mm_malloc(size)) != NULL){
		memcpy(new_bp,ptr,old_size); // Transportamos los datos entre punteros.
		mm_free(ptr);
	}

	mm_checkheap(__LINE__);
    return new_bp;
}
