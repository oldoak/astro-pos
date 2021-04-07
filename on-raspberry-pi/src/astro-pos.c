#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifdef __EMSCRIPTEN__
#include <GLES2/gl2.h>
#include <emscripten.h>
#else
#include <glad/glad.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <cglm/cglm.h>
#include <cglm/types.h>

const unsigned int DISP_WIDTH = 1200;
const unsigned int DISP_HEIGHT = 500;
const float PI = 3.14159265358979323846f;

GLFWwindow *window;
GLuint obj_shader_program;
GLuint spc_shader_program;

GLuint mv_mat_loc;
GLuint normal_mat_loc;
GLuint proj_mat_loc;
GLuint light_pos_loc;
GLuint ambient_col_loc;
GLuint diffuse_col_loc;

typedef struct AstroObject
{
    GLuint vbo;
    GLuint ebo;
    GLuint texture;
    GLuint vertexes_size;
    GLsizei indices_size;
    GLuint *indices;
    GLint object_pos;
    GLint object_texture;
    GLint object_normal;
} astro_object;

typedef struct AstroAttributes
{
    float positions[3];
    float textures[2];
    float normals[3];
} astro_attributes;

typedef struct GLData
{
    astro_object *earth;
    astro_object *moon;
    astro_object *space;
} gl_data;

typedef struct Image
{
    unsigned long sizeX;
    unsigned long sizeY;
    GLubyte *data;
} image;

static const char *filetobuf(const char *file)
{
    FILE *fptr;
    long length;
    char *buf;

    fptr = fopen(file, "rb");
    if (!fptr)
        return NULL;
    fseek(fptr, 0, SEEK_END);
    length = ftell(fptr);
    buf = (char *)malloc(length+1);
    fseek(fptr, 0, SEEK_SET);
    fread(buf, length, 1, fptr);
    fclose(fptr);
    buf[length] = 0;

    return buf;
}

static GLuint ShaderLoad(const char *filename, GLenum shader_type)
{
    const GLchar *shaderSrc = filetobuf(filename);
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &shaderSrc, NULL);

    // compile it
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if(!compiled)
    {
        // compilation failed - print error info
        fprintf(stderr, "Compilation of shader %s failed:\n", filename);
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        glDeleteShader(shader);
        shader = 0;
    }
    return shader;
}

static GLuint ShaderProgLoad(const char *vert_filename,
                             const char *frag_filename)
{
    GLuint vert_shader = ShaderLoad(vert_filename, GL_VERTEX_SHADER);
    if(!vert_shader)
    {
        fprintf(stderr, "Couldn't load vertex shader: %s\n", vert_filename);
        glDeleteShader(vert_shader);
        vert_shader = 0;
        return 0;
    }

    GLuint frag_shader = ShaderLoad(frag_filename, GL_FRAGMENT_SHADER);
    if(!frag_shader)
    {
        fprintf(stderr, "Couldn't load fragment shader: %s\n", frag_filename);
        glDeleteShader(frag_shader);
        frag_shader = 0;
        return 0;
    }

    GLuint shader_prog = glCreateProgram();
    if(shader_prog)
    {
        glAttachShader(shader_prog, vert_shader);
        glAttachShader(shader_prog, frag_shader);
        glLinkProgram(shader_prog);
        GLint linked = GL_FALSE;
        glGetProgramiv(shader_prog, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            fprintf(stderr,
                    "Linking shader failed (vert: %s, frag: %s\n",
                    vert_filename,
                    frag_filename);
            GLint logLength = 0;
            glGetProgramiv(shader_prog, GL_INFO_LOG_LENGTH, &logLength);

            fprintf(stderr, "Couldn't get shader link log; out of memory\n");
            glDeleteProgram(shader_prog);
            shader_prog = 0;
        }
        glDetachShader(shader_prog, vert_shader);
        glDetachShader(shader_prog, frag_shader);
    }
    else
    {
        fprintf(stderr, "Couldn't create shader program\n");
    }

    // don't need these any more
    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);
    return shader_prog;
}

static
GLuint SetTexture(const char *texture_file)
{
    GLuint texture;
    texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture); 
    // set the texture wrapping parameters
    // set texture wrapping to GL_REPEAT (default wrapping method)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // load image, create texture and generate mipmaps
    int width, height, nrChannels;
    unsigned char *data = stbi_load(texture_file,
                                    &width,
                                    &height,
                                    &nrChannels,
                                    STBI_rgb_alpha);

    if (data)
    {
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA,
                     width,
                     height,
                     0,
                     GL_RGBA,
                     GL_UNSIGNED_BYTE,
                     data);
        glGenerateMipmap(GL_TEXTURE_3D);
    }
    else
    {
        fprintf(stderr, "Failed to load texture\n");
    }

    if ((width & (width - 1)) != 0 || (height & (height - 1)) != 0)
    {
        fprintf(stderr,
                "WARNING: texture %s is not power-of-2 dimensions\n",
                texture_file);
    }

    stbi_image_free(data);
    return texture;
}

static unsigned int getint(FILE *fp)
{
   unsigned int c, c1, c2, c3;
   c  = ((unsigned int)getc(fp));  // get 4 bytes
   c1 = ((unsigned int)getc(fp)) << 8;
   c2 = ((unsigned int)getc(fp)) << 16;
   c3 = ((unsigned int)getc(fp)) << 24;
   return c | c1 | c2 | c3;
}

// ensure that little endian shorts are read into memory correctl
// on big endian platforms
static unsigned short getshort(FILE* fp)
{
   unsigned short c, c1;
   // get 2 bytes
   c  = ((unsigned short)getc(fp));
   c1 = ((unsigned short)getc(fp)) << 8;
   return c | c1;
}

static
int ImageLoad(const char *filename, image* bmp_image)
{
   FILE *file;
   unsigned long size;          // size of the image in bytes.
   size_t i,j,k, linediff;        
   unsigned short int planes;   // number of planes in image (must be 1)
   unsigned short int bpp;      // number of bits per pixel (must be 24)
   char temp;                   // temporary storage for bgr-rgb conversion.

   // make sure the file is there
   if ((file = fopen(filename, "rb"))==NULL) {
      printf("File Not Found : %s\n",filename);
      return -1;
   }

   // seek through the bmp header, up to the width/height
   fseek(file, 18, SEEK_CUR);

   // read the width
   bmp_image->sizeX = getint (file);

   // read the height
   bmp_image->sizeY = getint (file);

   // calculate the size (assuming 24 bits or 3 bytes per pixel)
   // BMP lines are padded to the nearest double word boundary
   size = 4.0*ceil(bmp_image->sizeX*24.0/32.0) * bmp_image->sizeY ;

   // read the planes
   planes = getshort(file);
   if (planes != 1){
      printf("Planes from %s is not 1: %u\n", filename, planes);
      return -1;
   }

   // read the bpp
   bpp = getshort(file);
   if (bpp != 24) {
      printf("Bpp from %s is not 24: %u\n", filename, bpp);
      return 0;
   }

   // seek past the rest of the bitmap header
   fseek(file, 24, SEEK_CUR);

   // allocate space for the data
   bmp_image->data = (GLubyte *) malloc(size * sizeof(GLubyte));
   if (bmp_image->data == NULL) {
      printf("Error allocating memory for color-corrected image data");
      return -1;
   }

   // read the data
   i = fread(bmp_image->data, size, 1, file);
   if (i != 1) {
      printf("Error reading image data from %s.\n", filename);
      return -1;
   }

   // reverse all of the colors (bgr -> rgb)
   // calculate distance to 4 byte boundary for each line
   // if this distance is not 0, then there will be a color reversal error
   // unless correction for the distance on each line
   linediff = 4.0*ceil(bmp_image->sizeX*24.0/32.0) - bmp_image->sizeX*3.0;
   k = 0;
   for (j=0;j<bmp_image->sizeY;j++) {
      for (i=0;i<bmp_image->sizeX;i++) {
        temp = bmp_image->data[k];
        bmp_image->data[k] = bmp_image->data[k+2];
        bmp_image->data[k+2] = temp;
        k+=3;
      }
      k+= linediff;
   }

   return 0;
}

static
GLuint SetBMPTexture(const char *texture_file)
{
    // load texture
    image *image1 = (image *) malloc(sizeof(image));

    // allocate space for texture
    if (image1 == NULL) {
       printf("Error allocating space for image");
       exit(0);
    }
 
    // load picture from file
    if (ImageLoad(texture_file, image1) != 0) {
       exit(1);
    }
 
    GLuint texture;
    texture = 0;
 
    // create texture name and bind it as current
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture); // 2d texture (x and y size)
 
    // set texture parameters
    // scale linearly when image bigger than texture
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    // scale linearly when image smaller than texture
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
 
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    glTexImage2D(GL_TEXTURE_2D,     // 2D texture
         0,                         // level of detail 0 (normal)
         3,                            // 3 color components
         image1->sizeX,             // x size from image
         image1->sizeY,             // y size from image
         0,                            // border 0 (normal)
         GL_RGB,                    // rgb color data order
         GL_UNSIGNED_BYTE,          // color component types
         image1->data               // image data itself
       );
 
    return texture;
}

static 
void background(astro_object *gd)
{
    glUseProgram(spc_shader_program);

    const astro_attributes bg[] =
    {
        {{  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0, 0.0, 0.0 }},
        {{  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0, 0.0, 0.0 }},
        {{ -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0, 0.0, 0.0 }},
        {{ -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0, 0.0, 0.0 }}
    };

    GLuint indices[6] = {0, 1, 2, 2, 3, 0};

    gd->vbo = 0;
    gd->ebo = 0;
    glGenBuffers(1, &gd->vbo);
    glGenBuffers(1, &gd->ebo);

    glBindBuffer(GL_ARRAY_BUFFER, gd->vbo);

    // copy the vertex data in, and deactivate
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(bg),
                 bg,
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gd->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 6 * sizeof(GLuint),
                 indices,
                 GL_STATIC_DRAW);

    gd->object_pos = glGetAttribLocation(spc_shader_program,
                                         "vertex_position");
    gd->object_texture = glGetAttribLocation(spc_shader_program,
                                             "vertex_texture");

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void sphere(astro_object *gd,
            float radius,
            unsigned int stacks,
            unsigned int sectors,
            unsigned int offset)
{
    gd->vertexes_size = stacks * sectors * sizeof(astro_attributes);
    astro_attributes vertexes[gd->vertexes_size];

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, length_inv = 1.0f / radius;   // normal
    float s, t;                                     // texCoord

    float sector_step = 2 * PI / sectors;
    float stack_step = PI / stacks;
    float sector_angle, stack_angle;

    int count = 0;
    for(int i = 0; i <= stacks; i++)
    {
        stack_angle = PI / 2 - i * stack_step; // starting from pi/2 to -pi/2
        xy = radius * cosf(stack_angle);       // r * cos(u)
        z = radius * sinf(stack_angle);        // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal,
        // but different tex coords
        for(int j = 0; j <= sectors; j++)
        {
            sector_angle = j * sector_step;          // starting from 0 to 2pi

            // vertex position
            x = xy * cosf(sector_angle);             // r * cos(u) * cos(v)
            y = xy * sinf(sector_angle);             // r * cos(u) * sin(v)
            vertexes[count].positions[0] = x + offset;
            vertexes[count].positions[1] = y;
            vertexes[count].positions[2] = z + offset;

            // vertex tex coord between [0, 1]
            s = (float)j / sectors;
            t = (float)i / stacks;

            vertexes[count].textures[0] = s;
            vertexes[count].textures[1] = t;

            // normalized vertex normal
            nx = x * length_inv;
            ny = y * length_inv;
            nz = z * length_inv;
            vertexes[count].normals[0] = nx;
            vertexes[count].normals[1] = ny;
            vertexes[count].normals[2] = nz;

            count++;
        }
    }

    // generate the index array
    gd->indices_size = ((stacks * sectors) - sectors) * 6;
    gd->indices = (GLuint *) malloc(gd->indices_size * sizeof(GLuint));

    unsigned int k1, k2;
    count = 0;
    for(int i = 0; i < stacks; i++)
    {
        k1 = i * (sectors + 1);     // beginning of current stack
        k2 = k1 + sectors + 1;      // beginning of next stack

        for(int j = 0; j < sectors; j++, k1++, k2++)
        {
            // 2 triangles per sector excluding 1st and last stacks
            if(i != 0)
            {
                // k1---k2---k1+1
                gd->indices[count++] = k1;
                gd->indices[count++] = k2;
                gd->indices[count++] = k1+1;
            }

            if(i != (stacks-1))
            {
                // k1+1---k2---k2+1
                gd->indices[count++] = k1+1;
                gd->indices[count++] = k2;
                gd->indices[count++] = k2+1;
            }
        }
    }

    gd->vbo = 0;
    gd->ebo = 0;
    glGenBuffers(1, &gd->vbo);
    glGenBuffers(1, &gd->ebo);

    glBindBuffer(GL_ARRAY_BUFFER, gd->vbo);

    // copy the vertex data in, and deactivate
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(vertexes),
                 vertexes,
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gd->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 gd->indices_size * sizeof(GLuint),
                 gd->indices,
                 GL_STATIC_DRAW);

    gd->object_pos = glGetAttribLocation(obj_shader_program, "vertex_position");
    gd->object_texture = glGetAttribLocation(obj_shader_program,
                                             "vertex_texture");
    gd->object_normal = glGetAttribLocation(obj_shader_program, "vertex_normal");

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void active_object(astro_object *gd)
{
    glBindBuffer(GL_ARRAY_BUFFER, gd->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gd->ebo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gd->texture);

    glEnableVertexAttribArray(gd->object_pos);
    glEnableVertexAttribArray(gd->object_texture);
    glEnableVertexAttribArray(gd->object_normal);

    glVertexAttribPointer(gd->object_pos,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(astro_attributes),
                          (const GLvoid*)0);

    glVertexAttribPointer(gd->object_texture,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(astro_attributes),
                          (const GLvoid*)offsetof(astro_attributes, textures));

    glVertexAttribPointer(gd->object_normal,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(astro_attributes),
                          (const GLvoid*)offsetof(astro_attributes, normals));
}

void active_background(astro_object *gd)
{
    glBindBuffer(GL_ARRAY_BUFFER, gd->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gd->ebo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gd->texture);

    glEnableVertexAttribArray(gd->object_pos);
    glEnableVertexAttribArray(gd->object_texture);

    glVertexAttribPointer(gd->object_pos,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(astro_attributes),
                          (const GLvoid*)0);

    glVertexAttribPointer(gd->object_texture,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(astro_attributes),
                          (const GLvoid*)offsetof(astro_attributes, textures));
}

void inactive_object(astro_object *gd)
{
    glDisableVertexAttribArray(gd->object_pos);
    glDisableVertexAttribArray(gd->object_texture);
    glDisableVertexAttribArray(gd->object_normal);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void inactive_background(astro_object *gd)
{
    glDisableVertexAttribArray(gd->object_pos);
    glDisableVertexAttribArray(gd->object_texture);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void planetoid(astro_object *gd,
               float radius,
               unsigned int stacks,
               unsigned int sectors,
               unsigned int offset)
{
    glUseProgram(obj_shader_program);
    glUniform1i(glGetUniformLocation(obj_shader_program, "texture"), 0);

    mv_mat_loc = glGetUniformLocation(obj_shader_program, "mv_mat");
    if (mv_mat_loc < 0)
    {
        fprintf(stderr, "ERROR: Couldn't get mv_mat's location.");
        exit(EXIT_FAILURE);
    }
    normal_mat_loc = glGetUniformLocation(obj_shader_program, "normal_mat");
    if (normal_mat_loc < 0)
    {
        fprintf(stderr, "ERROR: Couldn't get normal_mat's location.");
        exit(EXIT_FAILURE);
    }
    proj_mat_loc = glGetUniformLocation(obj_shader_program, "proj_mat");
    if (proj_mat_loc < 0)
    {
        fprintf(stderr, "ERROR: Couldn't get proj_mat's location.");
        exit(EXIT_FAILURE);
    }
    light_pos_loc = glGetUniformLocation(obj_shader_program, "light_position");
    if (light_pos_loc < 0)
    {
        fprintf(stderr, "ERROR: Couldn't get light_position's location.");
        exit(EXIT_FAILURE);
    }
    ambient_col_loc = glGetUniformLocation(obj_shader_program, "ambient_colour");
    if (ambient_col_loc < 0)
    {
        fprintf(stderr, "ERROR: Couldn't get ambient_colour's location.");
        exit(EXIT_FAILURE);
    }
    diffuse_col_loc = glGetUniformLocation(obj_shader_program, "diffuse_colour");
    if (diffuse_col_loc < 0)
    {
        fprintf(stderr, "ERROR: Couldn't get diffuse_colour's location.");
        exit(EXIT_FAILURE);
    }

    sphere(gd, radius, stacks, sectors, offset);
}

void draw(gl_data *gd)
{
    int width, height;

    glfwGetFramebufferSize(window, &width, &height);
 
    glViewport(0, 0, width, height);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glUseProgram(spc_shader_program);
    active_background(gd->space);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)0);
    inactive_background(gd->space);
    glUseProgram(0);
    glEnable(GL_DEPTH_TEST);

    glUseProgram(obj_shader_program);

    mat4 model_mat = GLM_MAT4_IDENTITY_INIT;
    float cam_pos_x = 0.0f;
    float cam_pos_y = 0.0f;
    float cam_pos_z = 150.0f;
    mat4 view_mat = GLM_MAT4_IDENTITY_INIT;

    float camX = sin(0.2*glfwGetTime()) * 100;
    float camZ = cos(0.2*glfwGetTime()) * 100;
    glm_lookat((vec3){camX, 0.0, camZ},
               (vec3){0.0, 0.0, 0.0},
               (vec3){0.0, 1.0, 0.0},
               view_mat);  

    mat4 proj_mat;
    glm_perspective(glm_rad(60.0f),
                    (float)DISP_WIDTH / (float)DISP_HEIGHT,
                    1.0f,
                    1000.f,
                    proj_mat);

    mat4 mv_mat;
    glm_mul(view_mat, model_mat, mv_mat);
    mat4 normal_mat;
    glm_mat4_inv(mv_mat, normal_mat);
    glUniformMatrix4fv(mv_mat_loc, 1, GL_FALSE, (GLfloat *) mv_mat);
    glUniformMatrix4fv(normal_mat_loc, 1, GL_FALSE, (GLfloat *) normal_mat);
    glUniformMatrix4fv(proj_mat_loc, 1, GL_FALSE, (GLfloat *) proj_mat);
    glUniform3fv(light_pos_loc,
                 1,
                 (GLfloat *) (vec3) {cam_pos_x + 50.0f,
                                     cam_pos_y + 80.0f,
                                     cam_pos_z});
    glUniform3fv(ambient_col_loc,
                 1,
                 (GLfloat *) (vec3) {0.85f, 0.85f, 0.85f});

    // draw and animate
    active_object(gd->earth);
    mat4 rot_model_mat;
    glm_rotate_x(model_mat, 90, rot_model_mat);

    mat4 r_model_mat;
    glm_mul(rot_model_mat, rot_model_mat, r_model_mat);
    glm_mul(view_mat, r_model_mat, mv_mat);
    glm_mat4_inv(mv_mat, normal_mat);
    glUniformMatrix4fv(mv_mat_loc, 1, GL_FALSE, (GLfloat *) mv_mat);
    glUniformMatrix4fv(normal_mat_loc, 1, GL_FALSE, (GLfloat *) normal_mat);
    glDrawElements(GL_TRIANGLES,
                   gd->earth->indices_size,
                   GL_UNSIGNED_INT,
                   (void *)0);
    inactive_object(gd->earth);
    
    active_object(gd->moon);
    glm_mul(model_mat, model_mat, r_model_mat);
    glm_mul(view_mat, r_model_mat, mv_mat);
    glm_mat4_inv(mv_mat, normal_mat);
    glUniformMatrix4fv(mv_mat_loc, 1, GL_FALSE, (GLfloat *) mv_mat);
    glUniformMatrix4fv(normal_mat_loc, 1, GL_FALSE, (GLfloat *) normal_mat);

    glDrawElements(GL_TRIANGLES,
                   gd->moon->indices_size,
                   GL_UNSIGNED_INT,
                   (void *)0);
    inactive_object(gd->moon);
        
    glfwSwapBuffers(window);
    glfwPollEvents();
}

int main()
{
    if (!glfwInit())
        exit(EXIT_FAILURE);

    window = glfwCreateWindow(DISP_WIDTH,
                              DISP_HEIGHT,
                              "Earth and Moon Rotation",
                              NULL,
                              NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);

    #ifndef __EMSCRIPTEN__
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD");
        exit(EXIT_FAILURE);
    }
    #endif

    // load the shader program and set it for use
    spc_shader_program = ShaderProgLoad("textures/spc_texture.vert",
                                        "textures/spc_texture.frag");

    if(!spc_shader_program)
    {
        fprintf(stderr, "Couldn't create a shader program.");
        return EXIT_FAILURE;
    }

    obj_shader_program = ShaderProgLoad("textures/texture.vert",
                                        "textures/texture.frag");

    if(!obj_shader_program)
    {
        fprintf(stderr, "Couldn't create a shader program.");
        return EXIT_FAILURE;
    }

    gl_data gld;

    gld.space = (astro_object *) malloc(sizeof(astro_object));
    gld.earth = (astro_object *) malloc(sizeof(astro_object));
    gld.moon = (astro_object *) malloc(sizeof(astro_object));

    gld.space->texture = SetTexture("textures/space.jpg");
    gld.earth->texture = SetTexture("textures/earth.jpg");
    // BMP texture, but JPG image looks better
    //gld.earth->texture = SetBMPTexture("textures/earth2048.bmp");
    gld.moon->texture = SetTexture("textures/moon.jpg");
    background(gld.space);
    planetoid(gld.earth, 30.0f, 72, 36, 0);
    planetoid(gld.moon, 5.0f, 72, 36, 50);

    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg((em_arg_callback_func) draw,
                                 &gld,
                                 0,
                                 1);
    #else
    while (!glfwWindowShouldClose(window))
    {
        draw(&gld);
    }
    #endif

    glDeleteTextures(1, &gld.earth->texture);
    glDeleteBuffers(1, &gld.earth->ebo);
    glDeleteBuffers(1, &gld.earth->vbo);
    free(gld.earth->indices);

    glDeleteTextures(1, &gld.moon->texture);
    glDeleteBuffers(1, &gld.moon->ebo);
    glDeleteBuffers(1, &gld.moon->vbo);
    free(gld.moon->indices);

    glDeleteProgram(obj_shader_program);
    glfwDestroyWindow(window);

    free(gld.earth);
    free(gld.moon);

    glfwTerminate();

    exit(EXIT_SUCCESS);
}
