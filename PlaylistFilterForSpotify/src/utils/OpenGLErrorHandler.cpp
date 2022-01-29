#include "OpenGLErrorHandler.h"

void printOpenGLErrors()
{
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        printf("opengl error occured!\n");
        printf("%#05x\n", err);
    }
}

void GLAPIENTRY
OpenGLMessageCallback( GLenum source,
                 GLenum type,
                 GLuint id,
                 GLenum severity,
                 GLsizei length,
                 const GLchar* message,
                 const void* userParam )
{
  fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}