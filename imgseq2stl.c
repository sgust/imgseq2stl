/* convert a sequence of images into a STL file */

/* coordinates used: x to the right, y to the back, z to the top */

//FIXME
// add scaling of output
// binary STL format or obj format
// multi-threading

#include <vips/vips.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <bsd/string.h>

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
struct object *resize(struct object *object, unsigned long int numtriangles)
{
	unsigned long int newsize = 0;

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
	int startx;

	w = vips_image_get_width(image);
	h = vips_image_get_height(image);
	region = vips_region_new(image);
	for(y = 0; y < h; y++) {
		rect.left = 0;
		rect.top = y;
		rect.width = w;
		rect.height = 1;
		if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
		startx = -1;
		for(x = 0; x < w; x++) {
			if (*VIPS_REGION_ADDR(region, x, y)) {
				/* current pixel is on */
				if (startx < 0) {
					/* start new sequence */
					startx = x;
				} else {
					if (((x - startx) > 10) || ((w-1) == x)) {
						/* 10 pixels on or reached end, create surface */
						if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
						object->triangles[object->free].a = packpoint(startx, y,   z);
						object->triangles[object->free].b = packpoint(startx, y+1, z);
						object->triangles[object->free].c = packpoint(x+1,    y,   z);
						object->triangles[object->free++].normal = nrm_down;
						object->triangles[object->free].a = packpoint(startx, y+1, z);
						object->triangles[object->free].b = packpoint(x+1,    y+1, z);
						object->triangles[object->free].c = packpoint(x+1,    y,   z);
						object->triangles[object->free++].normal = nrm_down;
						startx = -1;
					}
				}
			} else {
				/* current pixel is off, create surface */
				if (startx >= 0) {
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(startx, y,   z);
					object->triangles[object->free].b = packpoint(startx, y+1, z);
					object->triangles[object->free].c = packpoint(x,        y,   z);
					object->triangles[object->free++].normal = nrm_down;
					object->triangles[object->free].a = packpoint(startx, y+1, z);
					object->triangles[object->free].b = packpoint(x,      y+1, z);
					object->triangles[object->free].c = packpoint(x,      y,   z);
					object->triangles[object->free++].normal = nrm_down;
					startx = -1;
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
					object->triangles[object->free].a = packpoint(x,   y,   z);
					object->triangles[object->free].b = packpoint(x,   y+1, z);
					object->triangles[object->free].c = packpoint(x+1, y,   z);
					object->triangles[object->free++].normal = nrm_down;
					object->triangles[object->free].a = packpoint(x,   y+1, z);
					object->triangles[object->free].b = packpoint(x+1, y+1, z);
					object->triangles[object->free].c = packpoint(x+1, y,   z);
					object->triangles[object->free++].normal = nrm_down;
				}
			} else {
				if (!*VIPS_REGION_ADDR(region2, x, y)) {
					/* add top surface for lower object */
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(x,   y+1, z);
					object->triangles[object->free].b = packpoint(x,   y,   z);
					object->triangles[object->free].c = packpoint(x+1, y,   z);
					object->triangles[object->free++].normal = nrm_up;
					object->triangles[object->free].a = packpoint(x,   y+1, z);
					object->triangles[object->free].b = packpoint(x+1, y,   z);
					object->triangles[object->free].c = packpoint(x+1, y+1, z);
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
	int startx;

	w = vips_image_get_width(image);
	h = vips_image_get_height(image);
	region = vips_region_new(image);
	for(y = 0; y < h; y++) {
		rect.left = 0;
		rect.top = y;
		rect.width = w;
		rect.height = 1;
		if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
		startx = -1;
		for(x = 0; x < w; x++) {
			if (*VIPS_REGION_ADDR(region, x, y)) {
				/* current pixel is on */
				if (startx < 0) {
					/* start new sequence */
					startx = x;
				} else {
					if (((x - startx) > 10) || ((w-1) == x)) {
						/* 10 pixels on or reached end, create surface */
						if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
						object->triangles[object->free].a = packpoint(startx, y,   z+1);
						object->triangles[object->free].b = packpoint(x+1,    y,   z+1);
						object->triangles[object->free].c = packpoint(startx, y+1, z+1);
						object->triangles[object->free++].normal = nrm_up;
						object->triangles[object->free].a = packpoint(startx, y+1, z+1);
						object->triangles[object->free].b = packpoint(x+1,    y,   z+1);
						object->triangles[object->free].c = packpoint(x+1,    y+1, z+1);
						object->triangles[object->free++].normal = nrm_up;
						startx = -1;
					}
				}
			} else {
				/* current pixel is off, create surface */
				if (startx >= 0) {
					if ((object->free + 2) >= object->size) object = resize(object, object->size * 2);
					object->triangles[object->free].a = packpoint(startx, y,   z+1);
					object->triangles[object->free].b = packpoint(x,      y,   z+1);
					object->triangles[object->free].c = packpoint(startx, y+1, z+1);
					object->triangles[object->free++].normal = nrm_up;
					object->triangles[object->free].a = packpoint(startx, y+1, z+1);
					object->triangles[object->free].b = packpoint(x,      y,   z+1);
					object->triangles[object->free].c = packpoint(x,      y+1, z+1);
					object->triangles[object->free++].normal = nrm_up;
					startx = -1;
				}
			}
		}

	}
	g_object_unref(region);
	return object;
}

/* dump all triangles as ASCII STL */
void dumptriangles_ascii(FILE *file, struct triangle *triangles, size_t size)
{
	int i;
	int count = 0;

	for(i = 0; i < size; i++) {
		if (triangles[i].a != 0xffffffffffffffff) {
			count++;
			fprintf(file, "facet normal ");
			switch (triangles[i].normal) {
				case nrm_front: fprintf(file, "0 -1 0"); break;
				case nrm_back: fprintf(file, "0 1 0"); break;
				case nrm_left: fprintf(file, "-1 0 0"); break;
				case nrm_right: fprintf(file, "1 0 0"); break;
				case nrm_up: fprintf(file, "0 0 1"); break;
				case nrm_down: fprintf(file, "0 0 -1"); break;
				default: fprintf(stderr, "internal error: illegal surface normal @%d\n", i); exit(1); break;
			}
			fprintf(file, "\n");
			fprintf(file, "outer loop\n");
			fprintf(file, "vertex %lu %lu %lu\n",
				triangles[i].a & 0xfffff,
				(triangles[i].a >> 20) & 0xfffff,
				(triangles[i].a >> 40) & 0xfffff
			);
			fprintf(file, "vertex %lu %lu %lu\n",
				triangles[i].b & 0xfffff,
				(triangles[i].b >> 20) & 0xfffff,
				(triangles[i].b >> 40) & 0xfffff
			);
			fprintf(file, "vertex %lu %lu %lu\n",
				triangles[i].c & 0xfffff,
				(triangles[i].c >> 20) & 0xfffff,
				(triangles[i].c >> 40) & 0xfffff
			);
			fprintf(file, "endloop\n");
			fprintf(file, "endfacet\n");
		}
	}
	fprintf(stderr, "%d triangles dumped\n", count);
}

int main(int argc, char *argv[])
{
	struct option longoptions[] = {
		{ "input", 1, NULL, 'i' },
		{ "output", 1, NULL, 'o' },
		{ "first", 1, NULL, 'f' },
		{ "last", 1, NULL, 'l' },
		{ 0, 0, 0, 0 }
	};
	char para_input[80];
	char para_output[80];
	int para_first = 0;
	int para_last = 0;
	struct object *object = NULL;
	VipsImage *image1 = NULL;
	VipsImage *image2 = NULL;
	int z;
	char s[80];
	FILE *file;

	/* parameter parsing */
	para_input[0] = 0;
	para_output[0] = 0;
	while(1) {
		int i;
		i = getopt_long(argc, argv, "", longoptions, NULL);
		if (i == -1) break;
		switch(i) {
			case 'i':
				strlcpy(para_input, optarg, sizeof(para_input));
				break;
			case 'o':
				strlcpy(para_output, optarg, sizeof(para_output));
				break;
			case 'f':
				para_first = strtol(optarg, NULL, 0);
				break;
			case 'l':
				para_last = strtol(optarg, NULL, 0);
				break;
		}
	}
	/* sanity checks */
	{
		int abort = 0;
		if (para_first < 0) { fprintf(stderr, "--first must be >= 0\n"); abort = 1; }
		if (para_last <= 0) { fprintf(stderr, "--last must be > 0\n"); abort = 1; }
		if (para_last <= para_first) { fprintf(stderr, "--last must be > --first\n"); abort = 1; }
		if (0 == strlen(para_input)) { fprintf(stderr, "--input must be set\n"); abort = 1; }
		if (0 == strlen(para_output)) { fprintf(stderr, "--output must be set\n"); abort = 1; }
		if (abort) exit(1);
	}

	if (VIPS_INIT (argv[0])) vips_error_exit("unable to start VIPS");

	file = fopen(para_output, "w");
	if (NULL == file) {
		fprintf(stderr, "Can't open output file for write\n");
		exit(1);
	}

	object = resize(NULL, 10);

	for(z = para_first; z <= para_last; z++) {
		fprintf(stderr, "\rWorking on layer %d", z); fflush(stderr);
		snprintf(s, sizeof(s), para_input, z);
		image1 = vips_image_new_from_file(s, NULL);
		if (NULL == image1) vips_error_exit("Can't load file");
		if (z == para_first) {
			/* first layer needs to have bottom added */
			object = addbottom(object, image1, z);
		} else {
			/* rest of the layers need z added */
			object = addz(object, image2, image1, z);
			g_object_unref(image2);
		}
		object = addfront(object, image1, z);
		object = addback(object, image1, z);
		object = addx(object, image1, z);
		object = addleft(object, image1, z);
		object = addright(object, image1, z);
		object = addy(object, image1, z);
		image2 = image1;
		if (z == para_last) {
			/* last layer needs top added */
			object = addtop(object, image1, z);
			g_object_unref(image1);
		}
	}
	fprintf(stderr, "\r                             \r"); fflush(stderr);

	fprintf(file, "solid %s\n", para_output);
	dumptriangles_ascii(file, object->triangles, object->size);
	fprintf(file, "endsolid %s\n", para_output);

	vips_shutdown();
	return 0;
}
