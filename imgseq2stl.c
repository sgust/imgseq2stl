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
	size_t free; /* first unused triangle */
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
struct layer *layer_resize(struct layer *layer, int numtriangles)
{
	int newsize = 0;

if (layer) fprintf(stderr, "layer_resize %x, %lu to %d\n", layer, layer->size, numtriangles);
else fprintf(stderr, "layer_resize init to %d\n", numtriangles);

	if (NULL == layer) {
		/* allocate size info and fill it */
		layer = malloc(sizeof(struct layer));
		layer->bytes = sizeof(struct layer);
		layer->size = 0;
		layer->free = 0;
	}
	newsize = sizeof(struct layer) + numtriangles * sizeof(struct triangle);
	layer = realloc(layer, newsize);
	if (layer->size < numtriangles) {
		/* need to initialise the new ones */
		memset(&layer->triangles[layer->size], 0xff, (numtriangles - layer->size) * sizeof(struct triangle));
	}
	layer->size = numtriangles;
	layer->bytes = newsize;
	return layer;
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
	return object;
}

/* add a voxel at position (x,y,z) into layer layer, expand as necessary */
struct layer *addvoxel(struct layer *layer, int x, int y, int z)
{
//fprintf(stderr, "addvoxel %x @(%d,%d,%d) %lu used\n", layer, x, y, z, layer->free);
	/* double size if too small */
	if ((layer->free + 12) >= layer->size) layer = layer_resize(layer, layer->size * 2);
	/* front side */
	layer->triangles[layer->free].a = packpoint(x,   y,   z);
	layer->triangles[layer->free].b = packpoint(x+1, y,   z);
	layer->triangles[layer->free].c = packpoint(x,   y,   z+1);
	layer->triangles[layer->free++].normal = nrm_front;
	layer->triangles[layer->free].a = packpoint(x,   y,   z+1);
	layer->triangles[layer->free].b = packpoint(x+1, y,   z);
	layer->triangles[layer->free].c = packpoint(x+1, y,   z+1);
	layer->triangles[layer->free++].normal = nrm_front;
	/* back side */
	layer->triangles[layer->free].a = packpoint(x,   y+1, z);
	layer->triangles[layer->free].b = packpoint(x,   y+1, z+1);
	layer->triangles[layer->free].c = packpoint(x+1, y+1, z);
	layer->triangles[layer->free++].normal = nrm_back;
	layer->triangles[layer->free].a = packpoint(x,   y+1, z+1);
	layer->triangles[layer->free].b = packpoint(x+1, y+1, z+1);
	layer->triangles[layer->free].c = packpoint(x+1, y+1, z);
	layer->triangles[layer->free++].normal = nrm_back;
	/* left side */
	layer->triangles[layer->free].a = packpoint(x,   y,   z);
	layer->triangles[layer->free].b = packpoint(x,   y,   z+1);
	layer->triangles[layer->free].c = packpoint(x,   y+1, z);
	layer->triangles[layer->free++].normal = nrm_left;
	layer->triangles[layer->free].a = packpoint(x,   y,   z+1);
	layer->triangles[layer->free].b = packpoint(x,   y+1, z+1);
	layer->triangles[layer->free].c = packpoint(x,   y+1, z);
	layer->triangles[layer->free++].normal = nrm_left;
	/* right side */
	layer->triangles[layer->free].a = packpoint(x+1, y,   z);
	layer->triangles[layer->free].b = packpoint(x+1, y+1, z);
	layer->triangles[layer->free].c = packpoint(x+1, y,   z+1);
	layer->triangles[layer->free++].normal = nrm_right;
	layer->triangles[layer->free].a = packpoint(x+1, y,   z+1);
	layer->triangles[layer->free].b = packpoint(x+1, y+1, z);
	layer->triangles[layer->free].c = packpoint(x+1, y+1, z+1);
	layer->triangles[layer->free++].normal = nrm_right;
	/* bottom side */
	layer->triangles[layer->free].a = packpoint(x,   y,   z);
	layer->triangles[layer->free].b = packpoint(x,   y+1, z);
	layer->triangles[layer->free].c = packpoint(x+1, y,   z);
	layer->triangles[layer->free++].normal = nrm_down;
	layer->triangles[layer->free].a = packpoint(x+1, y,   z);
	layer->triangles[layer->free].b = packpoint(x,   y+1, z);
	layer->triangles[layer->free].c = packpoint(x+1, y+1, z);
	layer->triangles[layer->free++].normal = nrm_down;
	/* top side */
	layer->triangles[layer->free].a = packpoint(x,   y,   z+1);
	layer->triangles[layer->free].b = packpoint(x+1, y,   z+1);
	layer->triangles[layer->free].c = packpoint(x,   y+1, z+1);
	layer->triangles[layer->free++].normal = nrm_up;
	layer->triangles[layer->free].a = packpoint(x+1, y,   z+1);
	layer->triangles[layer->free].b = packpoint(x+1, y+1, z+1);
	layer->triangles[layer->free].c = packpoint(x,   y+1, z+1);
	layer->triangles[layer->free++].normal = nrm_up;
	return layer;
}

/* remove any double triangles inside a layer (gets rid of inner partitions) */
void layer_uniq(struct layer *layer)
{
	int i, j;
	struct triangle *t1, *t2;

	for(i = 0, j = 1; i < (layer->free - 1);) {
		/* compare triangles i and j */
		if ((layer->triangles[i].a != 0xffffffffffffffff) && (layer->triangles[j].a != 0xffffffffffffffff)) {
			t1 = &layer->triangles[i];
			t2 = &layer->triangles[j];
			if ((t1->a == t2->a) && (t1->b == t2->b) && (t1->c == t2->c)) {
				t1->a = t1->b = t1->c = t2->a = t2->b = t2->c = 0xffffffffffffffff;
				i++;
				j = i+1;
			}
			else if ((t1->a == t2->a) && (t1->b == t2->c) && (t1->c == t2->b)) {
				t1->a = t1->b = t1->c = t2->a = t2->b = t2->c = 0xffffffffffffffff;
				i++;
				j = i+1;
			}
			else if ((t1->a == t2->b) && (t1->b == t2->a) && (t1->c == t2->c)) {
				t1->a = t1->b = t1->c = t2->a = t2->b = t2->c = 0xffffffffffffffff;
				i++;
				j = i+1;
			}
			else if ((t1->a == t2->b) && (t1->b == t2->c) && (t1->c == t2->b)) {
				t1->a = t1->b = t1->c = t2->a = t2->b = t2->c = 0xffffffffffffffff;
				i++;
				j = i+1;
			}
			else if ((t1->a == t2->c) && (t1->b == t2->a) && (t1->c == t2->b)) {
				t1->a = t1->b = t1->c = t2->a = t2->b = t2->c = 0xffffffffffffffff;
				i++;
				j = i+1;
			}
			else if ((t1->a == t2->c) && (t1->b == t2->b) && (t1->c == t2->a)) {
				t1->a = t1->b = t1->c = t2->a = t2->b = t2->c = 0xffffffffffffffff;
				i++;
				j = i+1;
			}
		}
		if (++j >= layer->free) {
			i++;
			j = i+1;
		}
	}
}

/* remove all unused triangles in a layer and free all unnecessary memory */
struct layer *layer_compress(struct layer *layer)
{
	int i, j;

	for(i = j = 0; j < layer->free; i++, j++) {
		while (0xffffffffffffffff == layer->triangles[j].a) {
			fprintf(stderr, "skip %d\n", j);
			j++;
		}
		if (i != j) {
			layer->triangles[i].normal = layer->triangles[j].normal;
			layer->triangles[i].a = layer->triangles[j].a;
			layer->triangles[i].b = layer->triangles[j].b;
			layer->triangles[i].c = layer->triangles[j].c;
		}
	}
	layer->free = i;
	layer = layer_resize(layer, i);
	return layer;
}

/* dump all triangles as ASCII STL */
//FIXME: use file handle, later use binary format
void dumptriangles(struct triangle *triangles, size_t size)
{
	int i;
	int count = 0;

	for(i = 0; i < size; i++) {
		if (triangles[i].a != 0xffffffffffffffff) {
			count++;
			printf("facet normal ");
			switch (triangles[i].normal) {
				case nrm_front: printf("0 -1 0"); break;
				case nrm_back: printf("0 1 0"); break;
				case nrm_left: printf("-1 0 0"); break;
				case nrm_right: printf("1 0 0"); break;
				case nrm_up: printf("0 0 1"); break;
				case nrm_down: printf("0 0 -1"); break;
				default: fprintf(stderr, "internal error: illegal surface normal @%d\n", i); exit(1); break;
			}
			printf("\n");
			printf("outer loop\n");
			printf("vertex %lu %lu %lu\n",
				triangles[i].a & 0xfffff,
				(triangles[i].a >> 20) & 0xfffff,
				(triangles[i].a >> 40) & 0xfffff
			);
			printf("vertex %lu %lu %lu\n",
				triangles[i].b & 0xfffff,
				(triangles[i].b >> 20) & 0xfffff,
				(triangles[i].b >> 40) & 0xfffff
			);
			printf("vertex %lu %lu %lu\n",
				triangles[i].c & 0xfffff,
				(triangles[i].c >> 20) & 0xfffff,
				(triangles[i].c >> 40) & 0xfffff
			);
			printf("endloop\n");
			printf("endfacet\n");
		}
	}
fprintf(stderr, "%d triangles dumped\n", count);
}

int main(int argc, char *argv[])
{
	struct layer *layer = NULL;
	struct object *object = NULL;

	object = object_resize(object, 10);
	layer = layer_resize(layer, 10);
	object->layers[0] = layer;
	layer = addvoxel(layer, 0, 0, 0);
	layer = addvoxel(layer, 1, 0, 0);
	layer_uniq(layer);
	layer = layer_compress(layer);

printf("solid test\n");
	dumptriangles(layer->triangles, layer->size);
}
