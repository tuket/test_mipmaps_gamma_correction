#include <stdio.h>
#include <stbi.h>

float compare_images(const char* fileName1, const char* fileName2)
{
    int w, h, c;
    unsigned char* a = stbi_load(fileName1, &w, &h, &c, 4);
    int w2, h2, c2;
    unsigned char* b = stbi_load(fileName2, &w2, &h2, &c2, 4);
    if(a == nullptr) {
        fprintf(stderr, "unable to open: %s\n", a);
        return -1;
    }
    if(b == nullptr) {
        fprintf(stderr, "unable to open: %s\n", b);
        return -1;
    }
    if(w != w2 || h != h2) {
        fprintf(stderr, "compre_images: images don't have the same dimension\n");
        return -1;
    }
    auto color = [&](const unsigned char* img, int x, int y, int c) { return img[c + 4*(x + w*y)]; };
    float error = 0;
    for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
    for(int c = 0; c < 4; c++)
    {
        error += abs(color(a, x, y, c) - color(b, x, y, c)) / 255.f;
    }
    return error;
}

int main(int argc, char* argv[])
{
    if(argc != 3) {
        printf("you need to privide the name of two files\n");
        return -1;
    }
    printf("%f", compare_images(argv[1], argv[2]));
}