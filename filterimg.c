/* filters an image to avoid non-manifold edges in a layer */

#include <vips/vips.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <bsd/string.h>

int main(int argc, char *argv[])
{
	struct option longoptions[] = {
		{ "input", 1, NULL, 'i' },
		{ "output", 1, NULL, 'o' },
		{ 0, 0, 0, 0 }
	};
	char para_input[80];
	char para_output[80];
	VipsImage *image = NULL;
	VipsRegion *region = NULL;
	VipsRect rect;
	int w, h, x, y;
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
		}
	}
	if (VIPS_INIT (argv[0])) vips_error_exit("unable to start VIPS");

	file = fopen(para_output, "w");
	if (NULL == file) {
		fprintf(stderr, "Can't open output file for write\n");
		exit(1);
	}

	image = vips_image_new_from_file(para_input, NULL);
	w = vips_image_get_width(image);
	h = vips_image_get_height(image);
	region = vips_region_new(image);
	rect.left = 0;
	rect.top = 0;
	rect.width = w;
	rect.height = h;
	if (vips_region_prepare(region, &rect) < 0) vips_error_exit("Can't prepare region");
	for(y = 0; y < h-1; y++) {
		for(x = 0; x < w-1; x++) {
			if (*VIPS_REGION_ADDR(region, x, y) == *VIPS_REGION_ADDR(region, x+1, y+1)) {
				if (*VIPS_REGION_ADDR(region, x+1, y) == *VIPS_REGION_ADDR(region, x, y+1)) {
					VIPS_REGION_ADDR(region, x, y)[0] = 0;
					VIPS_REGION_ADDR(region, x, y)[1] = 0;
					VIPS_REGION_ADDR(region, x, y)[2] = 0;
					VIPS_REGION_ADDR(region, x+1, y)[0] = 0;
					VIPS_REGION_ADDR(region, x+1, y)[1] = 0;
					VIPS_REGION_ADDR(region, x+1, y)[2] = 0;
					VIPS_REGION_ADDR(region, x, y+1)[0] = 0;
					VIPS_REGION_ADDR(region, x, y+1)[1] = 0;
					VIPS_REGION_ADDR(region, x, y+1)[2] = 0;
					VIPS_REGION_ADDR(region, x+1, y+1)[0] = 0;
					VIPS_REGION_ADDR(region, x+1, y+1)[1] = 0;
					VIPS_REGION_ADDR(region, x+1, y+1)[2] = 0;
				}
			}
		}
	}

	if (vips_image_write_to_file(image, para_output, NULL) < 0) vips_error_exit("Can't write image");

	g_object_unref(region);
	g_object_unref(image);

	vips_shutdown();
	return 0;
}
