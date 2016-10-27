#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <stddef.h>     /* offsetof */
#include <sys/time.h>
#include <math.h>

#define CREATOR "AlexBudau"
#define RGB_COMPONENT_COLOR 255
#define MAX_FILE_NAME_SIZE 60
#define OUTPUT_FILE "outmpi.ppm"

#define plus_truncate_uchar(a, b) \
    if (((int)(a)) + (b) < 0) \
        (a) = 0; \
    else if (((int)(a)) + (b) > 255) \
        (a) = 255; \
    else \
        (a) += (b);

#define compute_disperse(channel, ch) \
    error = ((int)(channel)) - palette.table[index].ch;\
    plus_truncate_uchar(channel, (error*7) >> 4);\
    plus_truncate_uchar(channel, (error*3) >> 4);\
    plus_truncate_uchar(channel, (error*5) >> 4);\
    plus_truncate_uchar(channel, (error*1) >> 4);

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


RGBImage *readPPM(const char *filename, int proc_num) {

	RGBImage *img;
	
    //alloc memory form image
    img = (RGBImage *)malloc(sizeof(RGBImage));
    if (!img) {
         fprintf(stderr, "Unable to allocate memory\n");
         exit(1);
    }

    if (proc_num == 0) {
        
        char buff[16];
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
    }

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


void FloydSteinbergDitherMPI(RGBImage image, RGBPalette palette, int num_procs, int proc_num) {
    
    struct timeval t1, t2;
    double elapsedTime;

    unsigned char *pixels;
    unsigned char *proc_pixels;
    long size, k;
    PalettizedImage result;
    unsigned char *result_pixels;
    int distanceSquared, minDistanceSquared, Rdiff, Gdiff, Bdiff, error;
    unsigned char index, R, G, B, i;

    if (proc_num == 0) {
        
        printf("MPI ");
        // start timer
        gettimeofday(&t1, NULL); 
        result.width = image.width;
        result.height = image.height;
        result.pixels = (unsigned char*)malloc(sizeof(unsigned char) * result.width * result.height);
        
        pixels = (unsigned char*)malloc(sizeof(unsigned char) * image.width * image.height * 3);
        memcpy(pixels, image.pixels, sizeof(unsigned char) * image.width * image.height * 3);
    }

    size = image.width * image.height / num_procs;
    proc_pixels = (unsigned char*)malloc(sizeof(unsigned char) * size * 3);
    result_pixels = (unsigned char*)malloc(sizeof(unsigned char) * size);
    
    MPI_Scatter(pixels, size * 3, MPI_UNSIGNED_CHAR, proc_pixels, size * 3, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    for (k = 0; k < size; k++) {
        R = proc_pixels[k*3+0];
        G = proc_pixels[k*3+1];
        B = proc_pixels[k*3+2];

        // FindNearestColor

        minDistanceSquared = 255*255 + 255*255 + 255*255 + 1;
        for (i = 0; i < palette.size; i++) {
            Rdiff = ((int)R) - palette.table[i].R;
            Gdiff = ((int)G) - palette.table[i].G;
            Bdiff = ((int)B) - palette.table[i].B;
            distanceSquared = Rdiff*Rdiff + Gdiff*Gdiff + Bdiff*Bdiff;
            if (distanceSquared < minDistanceSquared) {
                minDistanceSquared = distanceSquared;
                index = i;
            }
        }

        {
        compute_disperse(proc_pixels[k*3+0], R);    
        compute_disperse(proc_pixels[k*3+1], G);
        compute_disperse(proc_pixels[k*3+2], B);
        }
        result_pixels[k] = index;
    }

    free(proc_pixels);
    free(result_pixels);

    MPI_Gather(result_pixels, size, MPI_UNSIGNED_CHAR,
        result.pixels, size, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

    if (proc_num == 0) {
        free(pixels);
        // stop timer
        gettimeofday(&t2, NULL);

        // compute and print the elapsed time in millisec
        elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
        elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;
        printf ("TIME = %lf\n", elapsedTime); 

        writePal(OUTPUT_FILE, palette, result, image);
    }
}


int main(int argc, char* argv[]){

    if (argc != 2) {
        printf("call <floyd> input\n");
        exit(1);
    }

    MPI_Init(NULL, NULL);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    char input[MAX_FILE_NAME_SIZE];

    RGBImage *image;
    RGBPalette palette;

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

    
    strcpy(input, argv[1]);
    
    image = readPPM(input, world_rank);

    MPI_Bcast(&image->width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&image->height, 1, MPI_INT, 0, MPI_COMM_WORLD);
     
    FloydSteinbergDitherMPI(*image, palette, world_size, world_rank);
    
    if (world_rank == 0) {
        free(image->pixels);
    }

    free(image);

    MPI_Finalize();
    return 0;
}
