#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <stddef.h>     /* offsetof */
#include <sys/time.h>
#include <math.h>
#include <pthread.h>

#define CREATOR "AlexBudau"
#define RGB_COMPONENT_COLOR 255
#define MAX_FILE_NAME_SIZE 60
#define OUTPUT_FILE "outmpithreads.ppm"

#define plus_truncate_uchar(a, b) \
    if (((int)(a)) + (b) < 0) \
        (a) = 0; \
    else if (((int)(a)) + (b) > 255) \
        (a) = 255; \
    else \
        (a) += (b);

#define compute_disperse(channel) \
    error = ((int)(color.channel)) - p->palette.table[index].channel; \
    plus_truncate_uchar(color.channel, (error*7) >> 4);\
    plus_truncate_uchar(color.channel, (error*3) >> 4);\
    plus_truncate_uchar(color.channel, (error*5) >> 4); \
    plus_truncate_uchar(color.channel, (error*1) >> 4);

typedef struct {
    unsigned char R, G, B;
} RGBTriple;

typedef struct {
    int size;
    RGBTriple* table;
} RGBPalette;

typedef struct {
    int width, height;
    RGBTriple* pixels;
} RGBImage;

typedef struct {
    int width, height;
    unsigned char* pixels;
} PalettizedImage;

typedef struct {
    long size;
    RGBTriple *pixels;
    unsigned char* result;
    RGBPalette palette;
} TParam;

RGBImage *readPPM(const char *filename) {

	char buff[16];
	RGBImage *img;
	FILE *fp;
	int c, rgb_comp_color;

	//open PPM file for reading
	fp = fopen(filename, "rb");
	if (!fp) {
	  fprintf(stderr, "Unable to open file '%s'\n", filename);
	  exit(1);
	}

	//read image format
	if (!fgets(buff, sizeof(buff), fp)) {
	  perror(filename);
	  exit(1);
	}

    //check the image format
    if (buff[0] != 'P' || buff[1] != '6') {
         fprintf(stderr, "Invalid image format (must be 'P6')\n");
         exit(1);
    }

    //alloc memory form image
    img = (RGBImage *)malloc(sizeof(RGBImage));
    if (!img) {
         fprintf(stderr, "Unable to allocate memory\n");
         exit(1);
    }

    //check for comments
    c = getc(fp);
    while (c == '#') {
    while (getc(fp) != '\n') ;
         c = getc(fp);
    }

    ungetc(c, fp);
    //read image size information
    if (fscanf(fp, "%d %d", &img->width, &img->height) != 2) {
         fprintf(stderr, "Invalid image size (error loading '%s')\n", filename);
         exit(1);
    }

    //read rgb component
    if (fscanf(fp, "%d", &rgb_comp_color) != 1) {
         fprintf(stderr, "Invalid rgb component (error loading '%s')\n", filename);
         exit(1);
    }

    //check rgb component depth
    if (rgb_comp_color != RGB_COMPONENT_COLOR) {
         fprintf(stderr, "'%s' does not have 8-bits components\n", filename);
         exit(1);
    }

    while (fgetc(fp) != '\n') ;
    //memory allocation for pixel data
    img->pixels = (RGBTriple*)malloc(img->width * img->height * sizeof(RGBTriple));

    if (!img->pixels) {
         fprintf(stderr, "Unable to allocate memory\n");
         exit(1);
    }

    //read pixel data from file
    
    if ((fread(img->pixels, 3 * img->width, img->height , fp))!= img->height) {
         fprintf(stderr, "Unable to load file\n");
         exit(1);
    }

    fclose(fp);
    return img;
}

void writePPM(const char *filename, RGBImage *img) {
    FILE *fp;
    //open file for output
    fp = fopen(filename, "wb");
    if (!fp) {
         fprintf(stderr, "Unable to open file '%s'\n", filename);
         exit(1);
    }

    //write the header file

    //image format
    fprintf(fp, "P6\n");

    //comments
    fprintf(fp, "# Created by %s\n",CREATOR);

    //image size
    fprintf(fp, "%d %d\n",img->width,img->height);

    // rgb component depth
    fprintf(fp, "%d\n",RGB_COMPONENT_COLOR);

    // pixel data
    fwrite(img->pixels, 3 * img->width, img->height, fp);
    fclose(fp);
}

void* FloydSteinbergDitherTask(void *params) {
    TParam *p = (TParam*)params;
    int distanceSquared, minDistanceSquared, Rdiff, Gdiff, Bdiff, error;
    unsigned char index, i;
    RGBTriple color;
    long k;

    for (k = 0; k < p->size; k++) {
        
        color = p->pixels[k];
        // FindNearestColor

        minDistanceSquared = 255*255 + 255*255 + 255*255 + 1;
        for (i = 0; i < 16; i++) {
            Rdiff = ((int)color.R) - p->palette.table[i].R;
            Gdiff = ((int)color.G) - p->palette.table[i].G;
            Bdiff = ((int)color.B) - p->palette.table[i].B;
            distanceSquared = Rdiff*Rdiff + Gdiff*Gdiff + Bdiff*Bdiff;
            if (distanceSquared < minDistanceSquared) {
                minDistanceSquared = distanceSquared;
                index = i;
            }
        }

        {
        compute_disperse(R);
        compute_disperse(G);
        compute_disperse(B);
        }
        p->result[k] = index;
    }

    return NULL;
}

PalettizedImage FloydSteinbergDitherThreads(RGBImage image, RGBPalette palette, int num_threads)
{
    PalettizedImage result;
    result.width = image.width;
    result.height = image.height;
    int i;
    long size;
    pthread_t threads[num_threads];
    RGBTriple *pixels;
    RGBTriple *table;
    TParam p[num_threads];

    result.pixels = (unsigned char *)malloc(sizeof(unsigned char) * result.width * result.height);
    size = result.width * result.height / num_threads;

    // threads vs OPEN MP
    // avantaj threaduri - pot separa accesul la image.pixels - exclusive read
    // avantaj threaduri - vizibilitate mai buna asupra variabilelor
    // dezavantaj threaduri - master thread creaza cate o copie pt tabela de pixeli
    // avantaj threaduri peste MPI - scrierea se face in paralel
    for (i = 0; i < num_threads; i++) {
        pixels = (RGBTriple*)malloc(size * sizeof(RGBTriple));
        memcpy(pixels, image.pixels + i*size, size * sizeof(RGBTriple));
        table = (RGBTriple*)malloc(sizeof(RGBTriple) * 16);
        memcpy(table, palette.table, sizeof(RGBTriple) * 16);

        p[i].size = size;
        p[i].pixels = pixels;
        p[i].result = result.pixels + i*size;
        p[i].palette.size = palette.size;
        p[i].palette.table = table;
        //FloydSteinbergDitherTask(&p);
        if (pthread_create(&threads[i], NULL, &FloydSteinbergDitherTask, &p[i]))
            perror("pthread_create");
    }

    for (i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL))
            perror("pthread_join");
    }

    return result;
}


void writePal(const char *filename, RGBPalette palette, PalettizedImage result, RGBImage image) {
    FILE *fp;
    //open file for output
    fp = fopen(filename, "wb");
    if (!fp) {
         fprintf(stderr, "Unable to open file '%s'\n", filename);
         exit(1);
    }

    //write the header file
    //image format
    fprintf(fp, "P6\n");

    //comments
    fprintf(fp, "# Created by %s\n",CREATOR);

    //image size
    fprintf(fp, "%d %d\n",result.width,result.height);

    // rgb component depth
    fprintf(fp, "%d\n",RGB_COMPONENT_COLOR);


    int x, y;
    for(y = 0; y < result.height; y++) {
        for(x = 0; x < result.width; x++) {
            fwrite(&palette.table[result.pixels[x + y*image.width]],
                   3, 1, fp);
        }
    }

    fclose(fp);
}

int main(int argc, char* argv[]){

    if (argc != 3) {
        printf("Call <floyd> <num_threads> <input>\n");
        exit(1);
    }
    
    int num_threads;
    char input[MAX_FILE_NAME_SIZE];
    struct timeval t1, t2;
    double elapsedTime;

    RGBImage *image;
    RGBPalette palette;
    PalettizedImage result;

    num_threads = atoi(argv[1]);

    palette.size = 16;
    palette.table = (RGBTriple*)malloc(sizeof(RGBTriple) * 16);
    palette.table[0].R = 149;
    palette.table[0].G = 91;
    palette.table[0].B = 110;
    palette.table[1].R = 176;
    palette.table[1].G = 116;
    palette.table[1].B = 137;
    palette.table[2].R = 17;
    palette.table[2].G = 11;
    palette.table[2].B = 15;
    palette.table[3].R = 63;
    palette.table[3].G = 47;
    palette.table[3].B = 69;
    palette.table[4].R = 93;
    palette.table[4].G = 75;
    palette.table[4].B = 112;
    palette.table[5].R = 47;
    palette.table[5].G = 62;
    palette.table[5].B = 24;
    palette.table[6].R = 76;
    palette.table[6].G = 90;
    palette.table[6].B = 55;
    palette.table[7].R = 190;
    palette.table[7].G = 212;
    palette.table[7].B = 115;
    palette.table[8].R = 160;
    palette.table[8].G = 176;
    palette.table[8].B = 87;
    palette.table[9].R = 116;
    palette.table[9].G = 120;
    palette.table[9].B = 87;
    palette.table[10].R = 245;
    palette.table[10].G = 246;
    palette.table[10].B = 225;
    palette.table[11].R = 148;
    palette.table[11].G = 146;
    palette.table[11].B = 130;
    palette.table[12].R = 200;
    palette.table[12].G = 195;
    palette.table[12].B = 180;
    palette.table[13].R = 36;
    palette.table[13].G = 32;
    palette.table[13].B = 27;
    palette.table[14].R = 87;
    palette.table[14].G = 54;
    palette.table[14].B = 45;
    palette.table[15].R = 121;
    palette.table[15].G = 72;
    palette.table[15].B = 72;

    strcpy(input, argv[2]);
    
    image = readPPM(input);

    printf("Threads ");
    // start timer
    gettimeofday(&t1, NULL);    
    result = FloydSteinbergDitherThreads(*image, palette, num_threads);
    // stop timer
    gettimeofday(&t2, NULL);

    // compute and print the elapsed time in millisec
    elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;
    printf ("TIME = %lf\n", elapsedTime); 
    writePal(OUTPUT_FILE, palette, result, *image);

    free(image->pixels);
    free(image);

    return 0;
}
