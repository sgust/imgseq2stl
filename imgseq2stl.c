/* convert a sequence of images into a STL file */

/* coordinates used: x to the right, y to the back, z to the top */

//FIXME
// add command line parameters
// add scaling of output

#include <vips/vips.h>

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

/* data for one triangle */
struct triangle {
	normals_t normal;
	point_t a;
	point_t b;
	point_t c;
};

/* out object */
struct object {
	size_t size; /* how many triangles are alloc'ed for us */
	size_t bytes; /* size in bytes alloc'ed for us */
	size_t free; /* first unused triangle */
	struct triangle triangles[];
};

/* pack a point into point_t format */
point_t packpoint(int x, int y, int z)
{
	return ((uint64_t) x) | (((uint64_t) y) << 20) | (((uint64_t) z) << 40);
}

/* resize a data structure, return NULL on error */
struct object *resize(struct object *object, int numtriangles)
{
	int newsize = 0;

	if (NULL == object) {
		/* allocate size info and fill it */
		object = malloc(sizeof(struct object));
		object->bytes = sizeof(struct object);
		object->size = 0;
		object->free = 0;
	}
	newsize = sizeof(struct object) + numtriangles * sizeof(struct triangle);
	object = realloc(object, newsize);
	if (object->size < numtriangles) {
		/* need to initialise the new ones */
		memset(&object->triangles[object->size], 0xff, (numtriangles - object->size) * sizeof(struct triangle));
	}
	object->size = numtriangles;
	object->bytes = newsize;
	return object;
}

/* add bottom surface */
struct object *addbottom(struct object *object, VipsImage *image, int z)
{
	VipsRegion *region = NULL;
	VipsRect rect;
	int w, h, x, y;

	w = vips_image_get_width(image);
	h = vips_image_get_height(image);
	region = vips_region_new(image);
	for(y = 0; y < h; y++) {
		rect.left = 0;
		rect.top = y;
		rect.width = w;
		rect.height = 1;
		if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
		for(x = 0; x < w; x++) {
			if (*VIPS_REGION_ADDR(region, x, y)) {
				if (0xff == *VIPS_REGION_ADDR(region, x, y)) {
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(x,   y,   z);
					object->triangles[object->free].b = packpoint(x,   y+1, z);
					object->triangles[object->free].c = packpoint(x+1, y,   z);
					object->triangles[object->free++].normal = nrm_down;
					object->triangles[object->free].a = packpoint(x,   y+1, z);
					object->triangles[object->free].b = packpoint(x+1, y+1, z);
					object->triangles[object->free].c = packpoint(x+1, y,   z);
					object->triangles[object->free++].normal = nrm_down;
				} else {
					fprintf(stderr, "warning: pixel neither black nor white @ (%d, %d, %d)\n", x, y, z);
				}
			}
		}
	}
	g_object_unref(region);
	return object;
}

/* add outer front surface */
struct object *addfront(struct object *object, VipsImage *image, int z)
{
	VipsRegion *region = NULL;
	VipsRect rect;
	int w, x;

	w = vips_image_get_width(image);
	region = vips_region_new(image);
	rect.left = 0;
	rect.top = 0;
	rect.width = w;
	rect.height = 1;
	if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
	for(x = 0; x < w; x++) {
		if (*VIPS_REGION_ADDR(region, x, 0)) {
			if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
			object->triangles[object->free].a = packpoint(x,   0,   z+1);
			object->triangles[object->free].b = packpoint(x,   0,   z);
			object->triangles[object->free].c = packpoint(x+1, 0,   z);
			object->triangles[object->free++].normal = nrm_front;
			object->triangles[object->free].a = packpoint(x,   0,   z+1);
			object->triangles[object->free].b = packpoint(x+1, 0,   z);
			object->triangles[object->free].c = packpoint(x+1, 0,   z+1);
			object->triangles[object->free++].normal = nrm_front;
		}
	}
	g_object_unref(region);
	return object;
}

/* add outer back surface */
struct object *addback(struct object *object, VipsImage *image, int z)
{
	VipsRegion *region = NULL;
	VipsRect rect;
	int w, h, x;

	w = vips_image_get_width(image);
	h = vips_image_get_height(image);
	region = vips_region_new(image);
	rect.left = 0;
	rect.top = h-1;
	rect.width = w;
	rect.height = 1;
	if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
	for(x = 0; x < w; x++) {
		if (*VIPS_REGION_ADDR(region, x, h-1)) {
			if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
			object->triangles[object->free].a = packpoint(x,   h, z);
			object->triangles[object->free].b = packpoint(x,   h, z+1);
			object->triangles[object->free].c = packpoint(x+1, h, z);
			object->triangles[object->free++].normal = nrm_back;
			object->triangles[object->free].a = packpoint(x,   h, z+1);
			object->triangles[object->free].b = packpoint(x+1, h, z+1);
			object->triangles[object->free].c = packpoint(x+1, h, z);
			object->triangles[object->free++].normal = nrm_back;
		}
	}
	g_object_unref(region);
	return object;
}

/* add inner front and back surfaces */
struct object *addx(struct object *object, VipsImage *image, int z)
{
	VipsRegion *region = NULL;
	VipsRect rect;
	int w, h, x, y;

	w = vips_image_get_width(image);
	h = vips_image_get_height(image);
	region = vips_region_new(image);
	for(y = 0; y < h-1; y++) {
		rect.left = 0;
		rect.top = y;
		rect.width = w;
		rect.height = 2;
		if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
		for(x = 0; x < w; x++) {
			if (!*VIPS_REGION_ADDR(region, x, y)) {
				if (*VIPS_REGION_ADDR(region, x, y+1)) {
					/* add front surface for voxel in behind row */
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(x,   y+1, z+1);
					object->triangles[object->free].b = packpoint(x,   y+1, z);
					object->triangles[object->free].c = packpoint(x+1, y+1, z);
					object->triangles[object->free++].normal = nrm_front;
					object->triangles[object->free].a = packpoint(x,   y+1, z+1);
					object->triangles[object->free].b = packpoint(x+1, y+1, z);
					object->triangles[object->free].c = packpoint(x+1, y+1, z+1);
					object->triangles[object->free++].normal = nrm_front;
				}
			} else {
				if (!*VIPS_REGION_ADDR(region, x, y+1)) {
					/* add back surface for voxel in front row */
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(x,   y+1, z);
					object->triangles[object->free].b = packpoint(x,   y+1, z+1);
					object->triangles[object->free].c = packpoint(x+1, y+1, z);
					object->triangles[object->free++].normal = nrm_back;
					object->triangles[object->free].a = packpoint(x,   y+1, z+1);
					object->triangles[object->free].b = packpoint(x+1, y+1, z+1);
					object->triangles[object->free].c = packpoint(x+1, y+1, z);
					object->triangles[object->free++].normal = nrm_back;
				}
			}
		}
	}
	g_object_unref(region);
	return object;
}

/* add outer left surface */
struct object *addleft(struct object *object, VipsImage *image, int z)
{
	VipsRegion *region = NULL;
	VipsRect rect;
	int h, y;

	h = vips_image_get_height(image);
	region = vips_region_new(image);
	rect.left = 0;
	rect.top = 0;
	rect.width = 1;
	rect.height = h;
	if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
	for(y = 0; y < h; y++) {
		if (*VIPS_REGION_ADDR(region, 0, y)) {
			if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
			object->triangles[object->free].a = packpoint(0, y,   z);
			object->triangles[object->free].b = packpoint(0, y,   z+1);
			object->triangles[object->free].c = packpoint(0, y+1, z);
			object->triangles[object->free++].normal = nrm_left;
			object->triangles[object->free].a = packpoint(0, y,   z+1);
			object->triangles[object->free].b = packpoint(0, y+1, z+1);
			object->triangles[object->free].c = packpoint(0, y+1, z);
			object->triangles[object->free++].normal = nrm_left;
		}
	}
	g_object_unref(region);
	return object;
}

/* add outer right surface */
struct object *addright(struct object *object, VipsImage *image, int z)
{
	VipsRegion *region = NULL;
	VipsRect rect;
	int w, h, y;

	w = vips_image_get_width(image);
	h = vips_image_get_height(image);
	region = vips_region_new(image);
	rect.left = w-1;
	rect.top = 0;
	rect.width = 1;
	rect.height = h;
	if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
	for(y = 0; y < h; y++) {
		if (*VIPS_REGION_ADDR(region, w-1, y)) {
			if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
			object->triangles[object->free].a = packpoint(w, y,   z);
			object->triangles[object->free].b = packpoint(w, y+1, z);
			object->triangles[object->free].c = packpoint(w, y,   z+1);
			object->triangles[object->free++].normal = nrm_right;
			object->triangles[object->free].a = packpoint(w, y,   z+1);
			object->triangles[object->free].b = packpoint(w, y+1, z);
			object->triangles[object->free].c = packpoint(w, y+1, z+1);
			object->triangles[object->free++].normal = nrm_right;
		}
	}
	g_object_unref(region);
	return object;
}

/* add inner left and right surfaces */
struct object *addy(struct object *object, VipsImage *image, int z)
{
	VipsRegion *region = NULL;
	VipsRect rect;
	int w, h, x, y;

	w = vips_image_get_width(image);
	h = vips_image_get_height(image);
	region = vips_region_new(image);
	for(x = 0; x < w-1; x++) {
		rect.left = x;
		rect.top = 0;
		rect.width = 2;
		rect.height = h;
		if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
		for(y = 0; y < h; y++) {
			if (!*VIPS_REGION_ADDR(region, x, y)) {
				if (*VIPS_REGION_ADDR(region, x+1, y)) {
					/* add left surface for voxel in 2nd row */
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(x+1, y,   z);
					object->triangles[object->free].b = packpoint(x+1, y,   z+1);
					object->triangles[object->free].c = packpoint(x+1, y+1, z);
					object->triangles[object->free++].normal = nrm_left;
					object->triangles[object->free].a = packpoint(x+1, y,   z+1);
					object->triangles[object->free].b = packpoint(x+1, y+1, z+1);
					object->triangles[object->free].c = packpoint(x+1, y+1, z);
					object->triangles[object->free++].normal = nrm_left;
				}
			} else {
				if (!*VIPS_REGION_ADDR(region, x+1, y)) {
					/* add right surface for voxel in first row */
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(x+1, y,   z);
					object->triangles[object->free].b = packpoint(x+1, y+1, z);
					object->triangles[object->free].c = packpoint(x+1, y,   z+1);
					object->triangles[object->free++].normal = nrm_right;
					object->triangles[object->free].a = packpoint(x+1, y,   z+1);
					object->triangles[object->free].b = packpoint(x+1, y+1, z);
					object->triangles[object->free].c = packpoint(x+1, y+1, z+1);
					object->triangles[object->free++].normal = nrm_right;
				}
			}
		}
	}
	g_object_unref(region);
	return object;
}

/* add inner top and bottom surfaces */
struct object *addz(struct object *object, VipsImage *image1, VipsImage *image2, int z)
{
	VipsRegion *region1 = NULL;
	VipsRegion *region2 = NULL;
	VipsRect rect;
	int w, h, x, y;

	w = vips_image_get_width(image1);
	h = vips_image_get_height(image1);
	if (vips_image_get_width(image2) != w) vips_error_exit("Images have different width");
	if (vips_image_get_height(image2) != h) vips_error_exit("Images have different height");
	region1 = vips_region_new(image1);
	region2 = vips_region_new(image2);
	for(y = 0; y < h; y++) {
		for(x = 0; x < w; x++) {
			rect.left = 0;
			rect.top = y;
			rect.width = w;
			rect.height = 1;
			if (vips_region_prepare(region1, &rect) < 0) vips_error_exit("Can't prepare region");
			if (vips_region_prepare(region2, &rect) < 0) vips_error_exit("Can't prepare region");
			if (!*VIPS_REGION_ADDR(region1, x, y)) {
				if (*VIPS_REGION_ADDR(region2, x, y)) {
					/* add bottom surface for upper object */
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(x,   y,   z+1);
					object->triangles[object->free].b = packpoint(x,   y+1, z+1);
					object->triangles[object->free].c = packpoint(x+1, y,   z+1);
					object->triangles[object->free++].normal = nrm_down;
					object->triangles[object->free].a = packpoint(x,   y+1, z+1);
					object->triangles[object->free].b = packpoint(x+1, y+1, z+1);
					object->triangles[object->free].c = packpoint(x+1, y,   z+1);
					object->triangles[object->free++].normal = nrm_down;
				}
			} else {
				if (!*VIPS_REGION_ADDR(region2, x, y)) {
					/* add top surface for lower object */
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(x,   y+1, z+1);
					object->triangles[object->free].b = packpoint(x,   y,   z+1);
					object->triangles[object->free].c = packpoint(x+1, y,   z+1);
					object->triangles[object->free++].normal = nrm_up;
					object->triangles[object->free].a = packpoint(x,   y+1, z+1);
					object->triangles[object->free].b = packpoint(x+1, y,   z+1);
					object->triangles[object->free].c = packpoint(x+1, y+1, z+1);
					object->triangles[object->free++].normal = nrm_up;
				}
			}
		}
	}
	g_object_unref(region1);
	g_object_unref(region2);
	return object;
}

/* add top surface */
struct object *addtop(struct object *object, VipsImage *image, int z)
{
	VipsRegion *region = NULL;
	VipsRect rect;
	int w, h, x, y;

	w = vips_image_get_width(image);
	h = vips_image_get_height(image);
	region = vips_region_new(image);
	for(y = 0; y < h; y++) {
		rect.left = 0;
		rect.top = y;
		rect.width = w;
		rect.height = 1;
		if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
		for(x = 0; x < w; x++) {
			if (*VIPS_REGION_ADDR(region, x, y)) {
				if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
				object->triangles[object->free].a = packpoint(x,   y+1, z+1);
				object->triangles[object->free].b = packpoint(x,   y,   z+1);
				object->triangles[object->free].c = packpoint(x+1, y,   z+1);
				object->triangles[object->free++].normal = nrm_up;
				object->triangles[object->free].a = packpoint(x,   y+1, z+1);
				object->triangles[object->free].b = packpoint(x+1, y,   z+1);
				object->triangles[object->free].c = packpoint(x+1, y+1, z+1);
				object->triangles[object->free++].normal = nrm_up;
			}
		}
	}
	g_object_unref(region);
	return object;
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
	struct object *object = NULL;
	VipsImage *image1 = NULL;
	VipsImage *image2 = NULL;
	int z;
	char s[80];

	if (VIPS_INIT (argv[0])) vips_error_exit("unable to start VIPS");

	object = resize(NULL, 10);

	snprintf(s, sizeof(s), "f-%06d.gif", 0);
	image1 = vips_image_new_from_file(s, NULL);
	if (NULL == image1) vips_error_exit("Can't load file");
	object = addbottom(object, image1, 0);
	for(z = 0; z < 999; z++) {
fprintf(stderr, "\rWorking on layer %d", z); fflush(stderr);
		object = addfront(object, image1, z);
		object = addback(object, image1, z);
		object = addx(object, image1, z);
		object = addleft(object, image1, z);
		object = addright(object, image1, z);
		object = addy(object, image1, z);
		snprintf(s, sizeof(s), "f-%06d.gif", z+1);
		image2 = vips_image_new_from_file(s, NULL);
		if (NULL == image2) vips_error_exit("Can't load file");
		object = addz(object, image1, image2, z);
		g_object_unref(image1);
		image1 = image2;
	}
	object = addfront(object, image1, z);
	object = addback(object, image1, z);
	object = addx(object, image1, z);
	object = addleft(object, image1, z);
	object = addright(object, image1, z);
	object = addy(object, image1, z);
	object = addtop(object, image1, z);

printf("solid test\n");
	dumptriangles(object->triangles, object->size);

	vips_shutdown();
	return 0;
}
