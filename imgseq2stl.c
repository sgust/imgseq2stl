/* convert a sequence of images into a STL file */

/* coordinates used: x to the right, y to the back, z to the top */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* we need only 6 different surface normals, only working on cubes */
typedef enum {
	nrm_front,
	nrm_back,
	nrm_left,
	nrm_right,
	nrm_up,
	nrm_down
} normals_t;

typedef uint64_t point_t; /* 20 bits x, 20 bits y, 20 bits z */

struct triangle {
	normals_t normal;
	point_t a;
	point_t b;
	point_t c;
};

/* one layer */
struct layer {
	size_t size; /* how many triangles are alloc'ed for us */
	size_t bytes; /* size in bytes alloc'ed for us */
	struct triangle triangles[];
};

/* all the layers */
struct object {
	size_t size; /* how many layers are alloc'ed for us */
	struct layer *layers[];
};

/* pack a point into point_t format */
point_t packpoint(int x, int y, int z)
{
	return ((uint64_t) x) | (((uint64_t) y) << 20) | (((uint64_t) z) << 40);
}

/* resize a layer data structure, return NULL on error */
struct layer *layer_resize(struct layer* layer, int numtriangles)
{
	int newsize = 0;

	if (NULL == layer) {
		/* allocate size info and fill it */
		layer = malloc(sizeof(struct layer));
		layer->bytes = sizeof(struct layer);
		layer->size = 0;
	}
	newsize = sizeof(struct layer) + numtriangles * sizeof(struct triangle);
	layer = realloc(layer, newsize);
	if (layer->size < numtriangles) {
		/* need to initialise the new ones */
		memset(&layer->triangles[layer->size], 0xff, (numtriangles - layer->size) * sizeof(struct triangle));
	}
	layer->size = numtriangles;
	layer->bytes = newsize;
}

/* resize the object structure, return NULL on error */
struct object *object_resize(struct object *object, int numlayers)
{
	int newsize = 0;

	if (NULL == object) {
		/* allocate size info and fill it */
		object = malloc(sizeof(struct object));
		object->size = 0;
	}
	newsize = sizeof(struct object) + numlayers * sizeof(struct layer *);
	object = realloc(object, newsize);
	if (object->size < numlayers) {
		/* need to initialise to new ones */
		memset(&object->layers[object->size], 0, (numlayers - object->size));
	}
	object->size = numlayers;
}

/* dump all triangles as ASCII STL */
//FIXME: use file handle, later use binary format
void dumptriangles(struct triangle *triangles, size_t size)
{
	int i;

	for(i = 0; i < size; i++) {
		if (triangles[i].a != 0xffffffffffffffff) {
			printf("facet normal ");
			switch (triangles[i].normal) {
				case nrm_front: printf("0 -1 0"); break;
				case nrm_back: printf("0 1 0"); break;
				case nrm_left: printf("-1 0 0"); break;
				case nrm_right: printf("1 0 0"); break;
				case nrm_up: printf("0 0 1"); break;
				case nrm_down: printf("0 0 -1"); break;
				default: fprintf(stderr, "internal error: illegal surface normal\n"); exit(1); break;
			}
			printf("\n");
			printf("outer loop\n");
			printf("vertex %d %d %d\n",
				triangles[i].a & 0xfffff,
				(triangles[i].a >> 20) & 0xfffff,
				(triangles[i].a >> 40) & 0xfffff
			);
			printf("vertex %d %d %d\n",
				triangles[i].b & 0xfffff,
				(triangles[i].b >> 20) & 0xfffff,
				(triangles[i].b >> 40) & 0xfffff
			);
			printf("vertex %d %d %d\n",
				triangles[i].c & 0xfffff,
				(triangles[i].c >> 20) & 0xfffff,
				(triangles[i].c >> 40) & 0xfffff
			);
			printf("endloop\n");
			printf("endfacet\n");
		}
	}
}

int main(char *argv, int argc)
{
	struct layer *layer = NULL;
	struct object *object = NULL;
	int i;

	object = object_resize(object, 10);
	layer = layer_resize(layer, 100);
	object->layers[0] = layer;
	i = 0;
	/* front side */
	layer->triangles[i].a = packpoint(0, 0, 0);
	layer->triangles[i].b = packpoint(1, 0, 0);
	layer->triangles[i].c = packpoint(0, 0, 1);
	layer->triangles[i++].normal = nrm_front;
	layer->triangles[i].a = packpoint(0, 0, 1);
	layer->triangles[i].b = packpoint(1, 0, 0);
	layer->triangles[i].c = packpoint(1, 0, 1);
	layer->triangles[i++].normal = nrm_front;
	/* back side */
	layer->triangles[i].a = packpoint(0, 1, 0);
	layer->triangles[i].b = packpoint(0, 1, 1);
	layer->triangles[i].c = packpoint(1, 1, 0);
	layer->triangles[i++].normal = nrm_back;
	layer->triangles[i].a = packpoint(0, 1, 1);
	layer->triangles[i].b = packpoint(1, 1, 1);
	layer->triangles[i].c = packpoint(1, 1, 0);
	layer->triangles[i++].normal = nrm_back;
	/* left side */
	layer->triangles[i].a = packpoint(0, 0, 0);
	layer->triangles[i].b = packpoint(0, 0, 1);
	layer->triangles[i].c = packpoint(0, 1, 0);
	layer->triangles[i++].normal = nrm_left;
	layer->triangles[i].a = packpoint(0, 0, 1);
	layer->triangles[i].b = packpoint(0, 1, 1);
	layer->triangles[i].c = packpoint(0, 1, 0);
	layer->triangles[i++].normal = nrm_left;
	/* right side */
	layer->triangles[i].a = packpoint(1, 0, 0);
	layer->triangles[i].b = packpoint(1, 1, 0);
	layer->triangles[i].c = packpoint(1, 0, 1);
	layer->triangles[i++].normal = nrm_right;
	layer->triangles[i].a = packpoint(1, 0, 1);
	layer->triangles[i].b = packpoint(1, 1, 0);
	layer->triangles[i].c = packpoint(1, 1, 1);
	layer->triangles[i++].normal = nrm_right;
	/* bottom side */
	layer->triangles[i].a = packpoint(0, 0, 0);
	layer->triangles[i].b = packpoint(0, 1, 0);
	layer->triangles[i].c = packpoint(1, 0, 0);
	layer->triangles[i++].normal = nrm_down;
	layer->triangles[i].a = packpoint(1, 0, 0);
	layer->triangles[i].b = packpoint(0, 1, 0);
	layer->triangles[i].c = packpoint(1, 1, 0);
	layer->triangles[i++].normal = nrm_down;
	/* top side */
	layer->triangles[i].a = packpoint(0, 0, 1);
	layer->triangles[i].b = packpoint(1, 0, 1);
	layer->triangles[i].c = packpoint(0, 1, 1);
	layer->triangles[i++].normal = nrm_up;
	layer->triangles[i].a = packpoint(1, 0, 1);
	layer->triangles[i].b = packpoint(1, 1, 1);
	layer->triangles[i].c = packpoint(0, 1, 1);
	layer->triangles[i++].normal = nrm_up;

printf("solid test\n");
	dumptriangles(layer->triangles, layer->size);
}
