/* convert a sequence of images into a STL file */

/* coordinates used: x to the right, y to the back, z to the top */

#include <vips/vips.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
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

/* thread worker job */
typedef enum {
	work_finished,	/* thread has finished */
	work_fblrxy,	/* front back left right x y */
	work_z		/* z */
} work_t;

/* thread job */
struct job {
	GThread *id;
	volatile work_t work;
	struct object *object;
	int z;
	VipsImage *image1;
	VipsImage *image2;
	uint8_t *refcnt1;
	uint8_t *refcnt2;
};

/* collect all data from all threads in Fractal */
struct object *Fractal = NULL;

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

/* combine two object data structures */
struct object *objcat(struct object *dst, struct object *src)
{
	if (!src->free) return dst;
	if (dst->size <= (dst->free + src->free)) {
		dst = resize(dst, 2 * (dst->free + src->free));
	}
	memcpy(&dst->triangles[dst->free], &src->triangles[0], src->free * sizeof(struct triangle));
	dst->free += src->free;
	return dst;
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

/* gets started as a new thread */
void *jobs_worker(void *data)
{
	struct job *job = data;

	switch (job->work) {
		case work_fblrxy:
			job->object = addfront(job->object, job->image1, job->z);
			job->object = addback(job->object, job->image1, job->z);
			job->object = addleft(job->object, job->image1, job->z);
			job->object = addright(job->object, job->image1, job->z);
			job->object = addx(job->object, job->image1, job->z);
			job->object = addy(job->object, job->image1, job->z);
			break;
		case work_z:
			job->object = addz(job->object, job->image2, job->image1, job->z);
			break;
		default:
			break;
	}
	vips_thread_shutdown();
	job->work = work_finished;
	return NULL;
}

/* collect results and cleanup after finished job, runs in main thread */
void jobs_end(struct job *job)
{
	/* wait for thread to end */
	(void) g_thread_join(job->id);
	job->id = NULL;
	/* copy triangles */
	Fractal = objcat(Fractal, job->object);
	free(job->object);
	job->object = NULL;
	/* do reference counting */
	if (job->image1 && (++(*job->refcnt1) > 2)) {
		g_object_unref(job->image1);
	}
	if (job->image2 && (++(*job->refcnt2) > 2)) {
		g_object_unref(job->image2);
	}
}

/* wait for a free job, runs in main thread */
int jobs_wait(struct job jobs[], int num)
{
	int i;

	while(1) {
		for(i = 0; i < num; i++) {
			if (work_finished == jobs[i].work) {
				if (jobs[i].id) {
					jobs_end(&jobs[i]);
				}
				return i;
			}
		}
		sleep(1); /* wait a bit to give threads more time to finish */
	}
	return -1; /* never reached */
}

/* start a new thread, runs in main thread */
void jobs_new(struct job *jobs, int threads, work_t work, int z, VipsImage *image1, VipsImage *image2, uint8_t *refcnt1, uint8_t *refcnt2)
{
	int j;

	j = jobs_wait(jobs, threads);
	jobs[j].work = work;
	jobs[j].object = resize(NULL, 10);
	jobs[j].z = z;
	jobs[j].image1 = image1;
	jobs[j].image2 = image2;
	jobs[j].refcnt1 = refcnt1;
	jobs[j].refcnt2 = refcnt2;
	jobs[j].id = vips_g_thread_new("imgseq2stl", &jobs_worker, &jobs[j]);
}

int main(int argc, char *argv[])
{
	struct option longoptions[] = {
		{ "input", 1, NULL, 'i' },
		{ "output", 1, NULL, 'o' },
		{ "first", 1, NULL, 'f' },
		{ "last", 1, NULL, 'l' },
		{ "threads", 1, NULL, 't' },
		{ 0, 0, 0, 0 }
	};
	char para_input[80];
	char para_output[80];
	int para_first = 0;
	int para_last = 0;
	int para_threads = 1;
	VipsImage *image1 = NULL;
	VipsImage *image2 = NULL;
	uint8_t *imgrefcnts = NULL; /* use counters for VipsImages, only used in main thread */
	int z;
	char s[80];
	FILE *file;
	struct job *jobs;
	int i;

	s[0] = 0;
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
			case 't':
				para_threads = strtol(optarg, NULL, 0);
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
		if (para_threads < 1) { fprintf(stderr, "--threads must be >= 1\n"); abort = 1; }
		if (para_threads > 200) { fprintf(stderr, "--threads must be <= 200\n"); abort = 1; }
		if (abort) exit(1);
	}

	if (VIPS_INIT (argv[0])) vips_error_exit("unable to start VIPS");

	/* allocate worker threads */
	jobs = calloc(para_threads, sizeof(struct job));
	if (NULL == jobs) {
		fprintf(stderr, "Can't allocate jobs\n");
		exit(1);
	}

	/* allocate reference counters for VipsImages */
	imgrefcnts = calloc(para_last - para_first + 1, 1);
	if (NULL == imgrefcnts) {
		fprintf(stderr, "Can't allocate imgrefcnt\n");
		exit(1);
	}

	/* output file */
	file = fopen(para_output, "w");
	if (NULL == file) {
		fprintf(stderr, "Can't open output file for write\n");
		exit(1);
	}

	/* allocate space for final object */
	Fractal = resize(NULL, 1024*1024);

	for(z = para_first; z <= para_last; z++) {
		fprintf(stderr, "\rWorking on layer %d", z); fflush(stderr);
		snprintf(s, sizeof(s), para_input, z);
		image1 = vips_image_new_from_file(s, NULL);
		if (NULL == image1) vips_error_exit("Can't load file '%s'", s);
		if (z == para_first) {
			/* first layer needs to have bottom added */
			Fractal = addbottom(Fractal, image1, z);
			imgrefcnts[z - para_first]++;
		} else {
			/* rest of the layers need z added */
			jobs_new(jobs, para_threads, work_z, z, image1, image2, &imgrefcnts[z - para_first], &imgrefcnts[z - para_first - 1]);
		}
		/* combine all jobs which need only one image */
		jobs_new(jobs, para_threads, work_fblrxy, z, image1, NULL, &imgrefcnts[z - para_first], NULL);
		image2 = image1;
		if (z == para_last) {
			/* last layer needs top added */
			Fractal = addtop(Fractal, image1, z);
			imgrefcnts[z - para_first]++;
		}
	}
	/* wait for all threads to end and collect results */
	for(i = 0; i < para_threads; i++) {
		if (jobs[i].id) {
			jobs_end(&jobs[i]);
		}
	}
	fprintf(stderr, "\r                             \r"); fflush(stderr);

	fprintf(file, "solid %s\n", para_output);
	dumptriangles_ascii(file, Fractal->triangles, Fractal->size);
	fprintf(file, "endsolid %s\n", para_output);

	/* sanity checking VipsImage use counters */
	for(i = 0; i < (para_last - para_first + 1); i++) {
		if (imgrefcnts[i] != 3) fprintf(stderr, "refcnt %d %d\n", i, imgrefcnts[i]);
	}

	vips_shutdown();
	return 0;
}
