#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <queue>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstdint>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// ============================================================================
// Tipos básicos y herramientas del programa
// ============================================================================

using Color = std::array<float, 4>;

struct Celda {
    int col = -1;
    int fila = -1;

    bool esValida() const { return col >= 0 && fila >= 0; }
};

enum class Herramienta : int {
    Linea = 0,
    Circulo = 1,
    PincelLibre = 2,
    Triangulo = 3,
    Cuadrado = 4,
    BotePintura = 5,
    Borrador = 6
};

// ============================================================================
// Matemáticas 2D y matrices 4x4 (orden por columnas para OpenGL)
// ============================================================================

namespace Matematicas2D {

    struct Punto2D {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Matriz4 {
        std::array<float, 16> valores{};

        static Matriz4 identidad() {
            Matriz4 mat;
            mat.valores[0] = 1.0f;
            mat.valores[5] = 1.0f;
            mat.valores[10] = 1.0f;
            mat.valores[15] = 1.0f;
            return mat;
        }

        static Matriz4 multiplicar(const Matriz4& a, const Matriz4& b) {
            Matriz4 resultado;
            for (int col = 0; col < 4; ++col) {
                for (int fila = 0; fila < 4; ++fila) {
                    float suma = 0.0f;
                    for (int k = 0; k < 4; ++k) {
                        suma += a.valores[k * 4 + fila] * b.valores[col * 4 + k];
                    }
                    resultado.valores[col * 4 + fila] = suma;
                }
            }
            return resultado;
        }

        static Matriz4 traslacion(float moverX, float moverY) {
            Matriz4 mat = identidad();
            mat.valores[12] = moverX;
            mat.valores[13] = moverY;
            return mat;
        }

        static Matriz4 rotacionZ(float radianes) {
            Matriz4 mat = identidad();
            const float coseno = std::cos(radianes);
            const float seno = std::sin(radianes);
            mat.valores[0] = coseno;  mat.valores[4] = -seno;
            mat.valores[1] = seno;    mat.valores[5] = coseno;
            return mat;
        }

        static Matriz4 escala(float escalaX, float escalaY) {
            Matriz4 mat = identidad();
            mat.valores[0] = escalaX;
            mat.valores[5] = escalaY;
            return mat;
        }

        static Matriz4 ortografica(float izq, float der, float abajo, float arriba, float cerca, float lejos) {
            Matriz4 mat;
            mat.valores[0] = 2.0f / (der - izq);
            mat.valores[5] = 2.0f / (arriba - abajo);
            mat.valores[10] = -2.0f / (lejos - cerca);
            mat.valores[12] = -(der + izq) / (der - izq);
            mat.valores[13] = -(arriba + abajo) / (arriba - abajo);
            mat.valores[14] = -(lejos + cerca) / (lejos - cerca);
            mat.valores[15] = 1.0f;
            return mat;
        }

        // Invierte la matriz de transformación 2D para saber qué celda tocó el ratón
        Matriz4 invertirAfin2D() const {
            const float a = valores[0];
            const float b = valores[4];
            const float c = valores[1];
            const float d = valores[5];
            const float tx = valores[12];
            const float ty = valores[13];

            const float determinante = a * d - b * c;
            if (std::abs(determinante) < 1e-7f) {
                return identidad();
            }

            const float invDet = 1.0f / determinante;
            Matriz4 inversa = identidad();
            inversa.valores[0] = d * invDet;
            inversa.valores[4] = -b * invDet;
            inversa.valores[1] = -c * invDet;
            inversa.valores[5] = a * invDet;
            inversa.valores[12] = -(inversa.valores[0] * tx + inversa.valores[4] * ty);
            inversa.valores[13] = -(inversa.valores[1] * tx + inversa.valores[5] * ty);
            return inversa;
        }

        Punto2D aplicarAPunto(const Punto2D& punto) const {
            return {
                valores[0] * punto.x + valores[4] * punto.y + valores[12],
                valores[1] * punto.x + valores[5] * punto.y + valores[13]
            };
        }
    };

} // namespace Matematicas2D

// ============================================================================
// Lienzo lógico: matriz 2D de filas por columnas
// ============================================================================

class LienzoCuadricula {
public:
    LienzoCuadricula() = default;

    void redimensionar(int nuevasCols, int nuevasFilas, const Color& colorFondo) {
        nuevasCols = std::max(1, nuevasCols);
        nuevasFilas = std::max(1, nuevasFilas);

        std::vector<std::vector<Color>> nuevaMatriz(
            static_cast<size_t>(nuevasFilas),
            std::vector<Color>(static_cast<size_t>(nuevasCols), colorFondo)
        );

        const int filasACopiar = std::min(numFilas, nuevasFilas);
        const int colsACopiar = std::min(numColumnas, nuevasCols);

        for (int f = 0; f < filasACopiar; ++f) {
            for (int c = 0; c < colsACopiar; ++c) {
                nuevaMatriz[f][c] = matrizColores[f][c];
            }
        }

        matrizColores = std::move(nuevaMatriz);
        numColumnas = nuevasCols;
        numFilas = nuevasFilas;
    }

    void limpiarTodo(const Color& color) {
        for (auto& filaActual : matrizColores) {
            std::fill(filaActual.begin(), filaActual.end(), color);
        }
    }

    bool estaDentro(int col, int fila) const {
        return col >= 0 && col < numColumnas && fila >= 0 && fila < numFilas;
    }

    void pintarCelda(int col, int fila, const Color& color) {
        if (estaDentro(col, fila)) {
            matrizColores[fila][col] = color;
        }
    }

    const Color& obtenerColor(int col, int fila) const {
        return matrizColores[fila][col];
    }

    int columnas() const { return numColumnas; }
    int filas() const { return numFilas; }

    static bool sonColoresIguales(const Color& c1, const Color& c2) {
        constexpr float margen = 0.01f;
        return std::abs(c1[0] - c2[0]) < margen &&
            std::abs(c1[1] - c2[1]) < margen &&
            std::abs(c1[2] - c2[2]) < margen &&
            std::abs(c1[3] - c2[3]) < margen;
    }

private:
    int numColumnas = 0;
    int numFilas = 0;
    std::vector<std::vector<Color>> matrizColores;
};

// ============================================================================
// Algoritmos de dibujo discreto sobre la cuadrícula
// ============================================================================

namespace AlgoritmosDibujo {

    // 1. Línea con algoritmo del punto medio (separado por pendiente suave y fuerte)
    void dibujarLinea(LienzoCuadricula& lienzo, Celda inicio, Celda fin, const Color& color) {
        int x0 = inicio.col, y0 = inicio.fila;
        int x1 = fin.col, y1 = fin.fila;

        const int distX = std::abs(x1 - x0);
        const int distY = std::abs(y1 - y0);
        const int pasoX = (x1 >= x0) ? 1 : -1;
        const int pasoY = (y1 >= y0) ? 1 : -1;

        if (distX >= distY) {
            int decision = 2 * distY - distX;
            const int incRecto = 2 * distY;
            const int incDiagonal = 2 * (distY - distX);

            int x = x0;
            int y = y0;
            for (int i = 0; i <= distX; ++i) {
                lienzo.pintarCelda(x, y, color);
                x += pasoX;
                if (decision < 0) {
                    decision += incRecto;
                }
                else {
                    y += pasoY;
                    decision += incDiagonal;
                }
            }
        }
        else {
            int decision = 2 * distX - distY;
            const int incRecto = 2 * distX;
            const int incDiagonal = 2 * (distX - distY);

            int x = x0;
            int y = y0;
            for (int i = 0; i <= distY; ++i) {
                lienzo.pintarCelda(x, y, color);
                y += pasoY;
                if (decision < 0) {
                    decision += incRecto;
                }
                else {
                    x += pasoX;
                    decision += incDiagonal;
                }
            }
        }
    }

    // 2. Círculo con algoritmo entero de Bresenham (d = 3 - 2r)
    void dibujarCirculo(LienzoCuadricula& lienzo, Celda centro, int radio, const Color& color, bool relleno) {
        if (radio < 0) return;
        if (radio == 0) {
            lienzo.pintarCelda(centro.col, centro.fila, color);
            return;
        }

        auto pintarFranja = [&](int colDesde, int colHasta, int fila) {
            if (colDesde > colHasta) std::swap(colDesde, colHasta);
            for (int c = colDesde; c <= colHasta; ++c) {
                lienzo.pintarCelda(c, fila, color);
            }
            };

        int x = 0;
        int y = radio;
        int d = 3 - 2 * radio;

        while (x <= y) {
            if (relleno) {
                pintarFranja(centro.col - x, centro.col + x, centro.fila + y);
                pintarFranja(centro.col - x, centro.col + x, centro.fila - y);
                pintarFranja(centro.col - y, centro.col + y, centro.fila + x);
                pintarFranja(centro.col - y, centro.col + y, centro.fila - x);
            }
            else {
                lienzo.pintarCelda(centro.col + x, centro.fila + y, color);
                lienzo.pintarCelda(centro.col - x, centro.fila + y, color);
                lienzo.pintarCelda(centro.col + x, centro.fila - y, color);
                lienzo.pintarCelda(centro.col - x, centro.fila - y, color);
                lienzo.pintarCelda(centro.col + y, centro.fila + x, color);
                lienzo.pintarCelda(centro.col - y, centro.fila + x, color);
                lienzo.pintarCelda(centro.col + y, centro.fila - x, color);
                lienzo.pintarCelda(centro.col - y, centro.fila - x, color);
            }

            if (d < 0) {
                d += 4 * x + 6;
            }
            else {
                d += 4 * (x - y) + 10;
                --y;
            }
            ++x;
        }
    }

    // 3. Cuadrado recorriendo sus límites mínimo y máximo
    void dibujarCuadrado(LienzoCuadricula& lienzo, Celda inicio, Celda esquina, const Color& color, bool relleno) {
        const int difX = esquina.col - inicio.col;
        const int difY = esquina.fila - inicio.fila;
        const int lado = std::max(std::abs(difX), std::abs(difY));

        const int colOpuesta = inicio.col + ((difX >= 0) ? lado : -lado);
        const int filaOpuesta = inicio.fila + ((difY >= 0) ? lado : -lado);

        const int colIzq = std::min(inicio.col, colOpuesta);
        const int colDer = std::max(inicio.col, colOpuesta);
        const int filaAbajo = std::min(inicio.fila, filaOpuesta);
        const int filaArriba = std::max(inicio.fila, filaOpuesta);

        if (relleno) {
            for (int f = filaAbajo; f <= filaArriba; ++f) {
                for (int c = colIzq; c <= colDer; ++c) {
                    lienzo.pintarCelda(c, f, color);
                }
            }
        }
        else {
            for (int c = colIzq; c <= colDer; ++c) {
                lienzo.pintarCelda(c, filaAbajo, color);
                lienzo.pintarCelda(c, filaArriba, color);
            }
            for (int f = filaAbajo; f <= filaArriba; ++f) {
                lienzo.pintarCelda(colIzq, f, color);
                lienzo.pintarCelda(colDer, f, color);
            }
        }
    }

    // 4. Triángulo usando funciones de borde dentro de su caja delimitadora
    void dibujarTriangulo(LienzoCuadricula& lienzo, Celda v0, Celda v1, Celda v2, const Color& color, bool relleno) {
        if (relleno) {
            auto productoCruz = [](int ax, int ay, int bx, int by, int px, int py) -> int {
                return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
                };

            const int areaDoble = productoCruz(v0.col, v0.fila, v1.col, v1.fila, v2.col, v2.fila);
            if (areaDoble != 0) {
                const int minCol = std::min({ v0.col, v1.col, v2.col });
                const int maxCol = std::max({ v0.col, v1.col, v2.col });
                const int minFila = std::min({ v0.fila, v1.fila, v2.fila });
                const int maxFila = std::max({ v0.fila, v1.fila, v2.fila });

                for (int f = minFila; f <= maxFila; ++f) {
                    for (int c = minCol; c <= maxCol; ++c) {
                        const int borde0 = productoCruz(v1.col, v1.fila, v2.col, v2.fila, c, f);
                        const int borde1 = productoCruz(v2.col, v2.fila, v0.col, v0.fila, c, f);
                        const int borde2 = productoCruz(v0.col, v0.fila, v1.col, v1.fila, c, f);

                        const bool dentroAntihorario = (areaDoble > 0) && (borde0 >= 0 && borde1 >= 0 && borde2 >= 0);
                        const bool dentroHorario = (areaDoble < 0) && (borde0 <= 0 && borde1 <= 0 && borde2 <= 0);

                        if (dentroAntihorario || dentroHorario) {
                            lienzo.pintarCelda(c, f, color);
                        }
                    }
                }
            }
        }

        dibujarLinea(lienzo, v0, v1, color);
        dibujarLinea(lienzo, v1, v2, color);
        dibujarLinea(lienzo, v2, v0, color);
    }

    // 5. Bote de pintura usando búsqueda por anchura (BFS) con cola
    void rellenarBoteBFS(LienzoCuadricula& lienzo, Celda semilla, const Color& colorNuevo) {
        if (!lienzo.estaDentro(semilla.col, semilla.fila)) return;

        const Color colorOriginal = lienzo.obtenerColor(semilla.col, semilla.fila);
        if (LienzoCuadricula::sonColoresIguales(colorOriginal, colorNuevo)) return;

        const int cols = lienzo.columnas();
        const int filas = lienzo.filas();
        std::vector<std::vector<uint8_t>> visitadas(
            static_cast<size_t>(filas),
            std::vector<uint8_t>(static_cast<size_t>(cols), 0)
        );

        std::queue<Celda> colaPendientes;
        colaPendientes.push(semilla);
        visitadas[semilla.fila][semilla.col] = 1;

        constexpr int dirCol[4] = { 1, -1, 0, 0 };
        constexpr int dirFila[4] = { 0, 0, 1, -1 };

        while (!colaPendientes.empty()) {
            const Celda actual = colaPendientes.front();
            colaPendientes.pop();

            lienzo.pintarCelda(actual.col, actual.fila, colorNuevo);

            for (int k = 0; k < 4; ++k) {
                const int sigCol = actual.col + dirCol[k];
                const int sigFila = actual.fila + dirFila[k];

                if (lienzo.estaDentro(sigCol, sigFila) && !visitadas[sigFila][sigCol]) {
                    visitadas[sigFila][sigCol] = 1;
                    if (LienzoCuadricula::sonColoresIguales(lienzo.obtenerColor(sigCol, sigFila), colorOriginal)) {
                        colaPendientes.push({ sigCol, sigFila });
                    }
                }
            }
        }
    }

} // namespace AlgoritmosDibujo

// ============================================================================
// Manejo de Shaders en OpenGL 3.3
// ============================================================================

class GestorShaders {
public:
    GestorShaders() = default;
    ~GestorShaders() { liberar(); }

    bool iniciarShaders() {
        const char* codigoVertex = R"glsl(
            #version 330 core
            layout (location = 0) in vec2 aPosXY;
            layout (location = 1) in vec4 aColorRGBA;
            uniform mat4 uMatrizMVP;
            out vec4 colorVertice;
            void main() {
                gl_Position = uMatrizMVP * vec4(aPosXY, 0.0, 1.0);
                colorVertice = aColorRGBA;
            }
        )glsl";

        const char* codigoFragment = R"glsl(
            #version 330 core
            in vec4 colorVertice;
            out vec4 colorFinal;
            void main() {
                colorFinal = colorVertice;
            }
        )glsl";

        GLuint vs = compilarEtapa(GL_VERTEX_SHADER, codigoVertex, "VERTEX");
        if (vs == 0) return false;

        GLuint fs = compilarEtapa(GL_FRAGMENT_SHADER, codigoFragment, "FRAGMENT");
        if (fs == 0) {
            glDeleteShader(vs);
            return false;
        }

        idPrograma = glCreateProgram();
        glAttachShader(idPrograma, vs);
        glAttachShader(idPrograma, fs);
        glLinkProgram(idPrograma);

        glDeleteShader(vs);
        glDeleteShader(fs);

        GLint enlazado = GL_FALSE;
        glGetProgramiv(idPrograma, GL_LINK_STATUS, &enlazado);
        if (enlazado == GL_FALSE) {
            GLint largoLog = 0;
            glGetProgramiv(idPrograma, GL_INFO_LOG_LENGTH, &largoLog);
            std::string mensajeError(static_cast<size_t>(std::max(1, largoLog)), '\0');
            glGetProgramInfoLog(idPrograma, largoLog, nullptr, &mensajeError[0]);
            std::cerr << "Error al enlazar el programa shader:\n" << mensajeError << std::endl;
            liberar();
            return false;
        }

        ubicacionMVP = glGetUniformLocation(idPrograma, "uMatrizMVP");
        return true;
    }

    void activarConMatriz(const Matematicas2D::Matriz4& mvp) const {
        glUseProgram(idPrograma);
        glUniformMatrix4fv(ubicacionMVP, 1, GL_FALSE, mvp.valores.data());
    }

    void liberar() {
        if (idPrograma != 0) {
            glDeleteProgram(idPrograma);
            idPrograma = 0;
        }
    }

private:
    GLuint compilarEtapa(GLenum tipoShader, const char* fuente, const char* nombre) {
        GLuint shader = glCreateShader(tipoShader);
        glShaderSource(shader, 1, &fuente, nullptr);
        glCompileShader(shader);

        GLint compilado = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compilado);
        if (compilado == GL_FALSE) {
            GLint largoLog = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &largoLog);
            std::string mensajeError(static_cast<size_t>(std::max(1, largoLog)), '\0');
            glGetShaderInfoLog(shader, largoLog, nullptr, &mensajeError[0]);
            std::cerr << "Error al compilar shader " << nombre << ":\n" << mensajeError << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint idPrograma = 0;
    GLint ubicacionMVP = -1;
};

// ============================================================================
// Dibujador de la malla en GPU con dos VBOs (posiciones y colores)
// ============================================================================

class DibujadorMalla {
public:
    DibujadorMalla() = default;
    ~DibujadorMalla() { liberarBuffers(); }

    void iniciarBuffers() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vboPosiciones);
        glGenBuffers(1, &vboColores);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vboPosiciones);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Matematicas2D::Punto2D), nullptr);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, vboColores);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Color), nullptr);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void subirLienzo(const LienzoCuadricula& lienzo, float tamCelda, float mitadAncho, float mitadAlto) {
        const int cols = lienzo.columnas();
        const int filas = lienzo.filas();
        const size_t totalVertices = static_cast<size_t>(cols) * static_cast<size_t>(filas) * 6;

        listaPosiciones.clear();
        listaColores.clear();
        listaPosiciones.reserve(totalVertices);
        listaColores.reserve(totalVertices);

        // Dejamos un 5% de margen por lado (90% interior) para que se vea la cuadrícula blanca
        const float margenBorde = tamCelda * 0.05f;

        for (int f = 0; f < filas; ++f) {
            const float yMin = -mitadAlto + static_cast<float>(f) * tamCelda + margenBorde;
            const float yMax = -mitadAlto + static_cast<float>(f + 1) * tamCelda - margenBorde;

            for (int c = 0; c < cols; ++c) {
                const float xMin = -mitadAncho + static_cast<float>(c) * tamCelda + margenBorde;
                const float xMax = -mitadAncho + static_cast<float>(c + 1) * tamCelda - margenBorde;
                const Color& colorCelda = lienzo.obtenerColor(c, f);

                // Triángulo 1 del cuadrito
                listaPosiciones.push_back({ xMin, yMin });
                listaPosiciones.push_back({ xMax, yMin });
                listaPosiciones.push_back({ xMax, yMax });

                // Triángulo 2 del cuadrito
                listaPosiciones.push_back({ xMin, yMin });
                listaPosiciones.push_back({ xMax, yMax });
                listaPosiciones.push_back({ xMin, yMax });

                for (int v = 0; v < 6; ++v) {
                    listaColores.push_back(colorCelda);
                }
            }
        }

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vboPosiciones);
        glBufferData(GL_ARRAY_BUFFER, listaPosiciones.size() * sizeof(Matematicas2D::Punto2D), listaPosiciones.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, vboColores);
        glBufferData(GL_ARRAY_BUFFER, listaColores.size() * sizeof(Color), listaColores.data(), GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void dibujar() const {
        if (listaPosiciones.empty()) return;
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(listaPosiciones.size()));
        glBindVertexArray(0);
    }

    void liberarBuffers() {
        if (vboColores != 0) { glDeleteBuffers(1, &vboColores);    vboColores = 0; }
        if (vboPosiciones != 0) { glDeleteBuffers(1, &vboPosiciones); vboPosiciones = 0; }
        if (vao != 0) { glDeleteVertexArrays(1, &vao);      vao = 0; }
    }

private:
    GLuint vao = 0;
    GLuint vboPosiciones = 0;
    GLuint vboColores = 0;
    std::vector<Matematicas2D::Punto2D> listaPosiciones;
    std::vector<Color> listaColores;
};

// ============================================================================
// Aplicación principal: ventana, eventos e interfaz ImGui
// ============================================================================

class AplicacionPaint {
public:
    int ejecutar() {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));

        if (!glfwInit()) {
            std::cerr << "No se pudo iniciar GLFW." << std::endl;
            return -1;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        ventana = glfwCreateWindow(anchoVentana, altoVentana, "Grilla Academica - Paint 2D + Dear ImGui", nullptr, nullptr);
        if (!ventana) {
            std::cerr << "No se pudo crear la ventana." << std::endl;
            glfwTerminate();
            return -1;
        }

        glfwMakeContextCurrent(ventana);
        glfwSwapInterval(1);

        glfwSetWindowUserPointer(ventana, this);
        glfwSetFramebufferSizeCallback(ventana, alCambiarTamano);
        glfwSetCursorPosCallback(ventana, alMoverRaton);
        glfwSetMouseButtonCallback(ventana, alHacerClic);
        glfwSetKeyCallback(ventana, alPresionarTecla);

        if (!gladLoadGL()) {
            std::cerr << "No se pudo cargar OpenGL con GLAD." << std::endl;
            glfwDestroyWindow(ventana);
            glfwTerminate();
            return -1;
        }

        iniciarImGui();

        if (!shaders.iniciarShaders()) {
            cerrarImGui();
            glfwDestroyWindow(ventana);
            glfwTerminate();
            return -1;
        }

        dibujador.iniciarBuffers();

        glfwGetFramebufferSize(ventana, &anchoVentana, &altoVentana);
        ajustarCuadricula();
        cargarEscenaInicial();

        while (!glfwWindowShouldClose(ventana)) {
            glfwPollEvents();
            renderizarFotograma();
        }

        dibujador.liberarBuffers();
        shaders.liberar();
        cerrarImGui();

        glfwDestroyWindow(ventana);
        glfwTerminate();
        return 0;
    }

private:
    // Colores del proyecto
    static constexpr Color COLOR_FONDO_ROSA = { 240.0f / 255.0f, 182.0f / 255.0f, 205.0f / 255.0f, 1.0f };
    static constexpr Color COLOR_AZUL_DEFECTO = { 39.0f / 255.0f,  87.0f / 255.0f, 245.0f / 255.0f, 1.0f };
    static constexpr Color COLOR_ROJO_DEFECTO = { 1.0f,            0.0f,            0.0f,          1.0f };

    static constexpr float TAMANO_CELDA = 0.25f;
    static constexpr float PIXELES_POR_UNIDAD = 60.0f;
    static constexpr float GRADOS_A_RADIANES = 3.14159265359f / 180.0f;

    GLFWwindow* ventana = nullptr;
    GestorShaders shaders;
    DibujadorMalla dibujador;
    LienzoCuadricula lienzo;

    int anchoVentana = 1000;
    int altoVentana = 700;
    float mitadAnchoMundo = 0.0f;
    float mitadAltoMundo = 0.0f;

    Color colorActual = COLOR_AZUL_DEFECTO;
    Herramienta herramientaActual = Herramienta::PincelLibre;
    bool rellenarFigura = false;
    bool actualizarGPU = true;

    bool arrastrando = false;
    Celda celdaInicio{};
    Celda celdaCursor{};
    Celda ultimaCeldaTrazo{};

    std::array<Celda, 3> puntosTriangulo{};
    int pasoTriangulo = 0;

    // Datos numéricos del menú desplegable
    int datosLinea[4] = { 5, 5, 25, 18 };
    int datosCirculo[3] = { 20, 15, 6 };
    int datosCuadrado[3] = { 10, 10, 8 };
    int datosTriangulo[6] = { 8, 8, 22, 8, 15, 20 };

    // Transformaciones 2D
    float moverX = 0.0f;
    float moverY = 0.0f;
    float rotacionGrados = 0.0f;
    float escalaZoom = 1.0f;

    void iniciarImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        aplicarTemaElegante();

        ImGui_ImplGlfw_InitForOpenGL(ventana, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void aplicarTemaElegante() {
        ImGuiStyle& estilo = ImGui::GetStyle();

        // Bordes redondeados y espaciado suave tipo editorial
        estilo.WindowRounding = 14.0f;
        estilo.ChildRounding = 10.0f;
        estilo.FrameRounding = 8.0f;
        estilo.PopupRounding = 10.0f;
        estilo.ScrollbarRounding = 10.0f;
        estilo.GrabRounding = 8.0f;

        estilo.WindowBorderSize = 1.0f;
        estilo.FrameBorderSize = 1.0f;
        estilo.WindowPadding = ImVec2(14.0f, 14.0f);
        estilo.FramePadding = ImVec2(10.0f, 6.0f);
        estilo.ItemSpacing = ImVec2(10.0f, 8.0f);

        ImVec4* colores = estilo.Colors;

        // Textos en tono ciruela oscuro
        colores[ImGuiCol_Text] = ImVec4(0.28f, 0.15f, 0.22f, 1.00f);
        colores[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.45f, 0.52f, 1.00f);

        // Fondo principal crema rosado perlado
        colores[ImGuiCol_WindowBg] = ImVec4(0.99f, 0.96f, 0.97f, 0.96f);
        colores[ImGuiCol_PopupBg] = ImVec4(0.99f, 0.95f, 0.97f, 0.98f);

        // Bordes y líneas divisoras en oro rosa suave
        colores[ImGuiCol_Border] = ImVec4(0.89f, 0.72f, 0.80f, 0.65f);
        colores[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colores[ImGuiCol_Separator] = ImVec4(0.89f, 0.72f, 0.80f, 0.60f);

        // Barra de título palo de rosa pastel
        colores[ImGuiCol_TitleBg] = ImVec4(0.95f, 0.82f, 0.87f, 1.00f);
        colores[ImGuiCol_TitleBgActive] = ImVec4(0.92f, 0.72f, 0.81f, 1.00f);
        colores[ImGuiCol_TitleBgCollapsed] = ImVec4(0.96f, 0.86f, 0.90f, 0.85f);

        // Cajas de texto, sliders y casillas en rosa pétalo
        colores[ImGuiCol_FrameBg] = ImVec4(0.96f, 0.88f, 0.91f, 1.00f);
        colores[ImGuiCol_FrameBgHovered] = ImVec4(0.94f, 0.81f, 0.87f, 1.00f);
        colores[ImGuiCol_FrameBgActive] = ImVec4(0.91f, 0.74f, 0.82f, 1.00f);

        // Botones en rosa empolvado chic
        colores[ImGuiCol_Button] = ImVec4(0.92f, 0.68f, 0.78f, 1.00f);
        colores[ImGuiCol_ButtonHovered] = ImVec4(0.87f, 0.57f, 0.70f, 1.00f);
        colores[ImGuiCol_ButtonActive] = ImVec4(0.81f, 0.47f, 0.62f, 1.00f);

        // Encabezados desplegables
        colores[ImGuiCol_Header] = ImVec4(0.94f, 0.77f, 0.84f, 0.90f);
        colores[ImGuiCol_HeaderHovered] = ImVec4(0.90f, 0.67f, 0.77f, 1.00f);
        colores[ImGuiCol_HeaderActive] = ImVec4(0.85f, 0.57f, 0.69f, 1.00f);

        // Acentos (checks, bolitas de radio y controles deslizantes) en frambuesa suave
        colores[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.30f, 0.52f, 1.00f);
        colores[ImGuiCol_SliderGrab] = ImVec4(0.84f, 0.42f, 0.60f, 1.00f);
        colores[ImGuiCol_SliderGrabActive] = ImVec4(0.76f, 0.28f, 0.49f, 1.00f);

        // Barra de desplazamiento
        colores[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.93f, 0.95f, 0.60f);
        colores[ImGuiCol_ScrollbarGrab] = ImVec4(0.89f, 0.70f, 0.78f, 1.00f);
        colores[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.84f, 0.58f, 0.69f, 1.00f);
        colores[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.78f, 0.46f, 0.60f, 1.00f);
    }

    void cerrarImGui() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ajustarCuadricula() {
        const float anchoMundo = static_cast<float>(anchoVentana) / PIXELES_POR_UNIDAD;
        const float altoMundo = static_cast<float>(altoVentana) / PIXELES_POR_UNIDAD;
        mitadAnchoMundo = anchoMundo * 0.5f;
        mitadAltoMundo = altoMundo * 0.5f;

        const int colsNecesarias = static_cast<int>(std::ceil(anchoMundo / TAMANO_CELDA));
        const int filasNecesarias = static_cast<int>(std::ceil(altoMundo / TAMANO_CELDA));

        lienzo.redimensionar(colsNecesarias, filasNecesarias, COLOR_FONDO_ROSA);
        actualizarGPU = true;
    }

    void cargarEscenaInicial() {
        lienzo.limpiarTodo(COLOR_FONDO_ROSA);
        AlgoritmosDibujo::dibujarCirculo(lienzo, { 15, 27 }, 7, COLOR_AZUL_DEFECTO, false);
        AlgoritmosDibujo::dibujarLinea(lienzo, { 7, 10 }, { 31, 20 }, COLOR_ROJO_DEFECTO);
        pasoTriangulo = 0;
        actualizarGPU = true;
    }

    Matematicas2D::Matriz4 calcularMatrizModelo() const {
        const auto t = Matematicas2D::Matriz4::traslacion(moverX, moverY);
        const auto r = Matematicas2D::Matriz4::rotacionZ(rotacionGrados * GRADOS_A_RADIANES);
        const auto s = Matematicas2D::Matriz4::escala(escalaZoom, escalaZoom);
        return Matematicas2D::Matriz4::multiplicar(t, Matematicas2D::Matriz4::multiplicar(r, s));
    }

    bool ratonACelda(double ratonX, double ratonY, Celda& celdaSalida) const {
        if (anchoVentana <= 0 || altoVentana <= 0 || escalaZoom <= 0.0001f) {
            return false;
        }

        const float normX = (2.0f * static_cast<float>(ratonX) / static_cast<float>(anchoVentana)) - 1.0f;
        const float normY = 1.0f - (2.0f * static_cast<float>(ratonY) / static_cast<float>(altoVentana));

        const Matematicas2D::Punto2D puntoMundo{ normX * mitadAnchoMundo, normY * mitadAltoMundo };
        const Matematicas2D::Matriz4 matrizInversa = calcularMatrizModelo().invertirAfin2D();
        const Matematicas2D::Punto2D puntoLocal = matrizInversa.aplicarAPunto(puntoMundo);

        celdaSalida.col = static_cast<int>(std::floor((puntoLocal.x + mitadAnchoMundo) / TAMANO_CELDA));
        celdaSalida.fila = static_cast<int>(std::floor((puntoLocal.y + mitadAltoMundo) / TAMANO_CELDA));

        return lienzo.estaDentro(celdaSalida.col, celdaSalida.fila);
    }

    void dibujarPanelImGui() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Ancho de 370 px para que ninguna etiqueta quede cortada a la derecha
        ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(370, 610), ImGuiCond_FirstUseEver);
        ImGui::Begin("Herramientas Paint 2D");

        const ImVec4 tonoAcento(0.72f, 0.24f, 0.46f, 1.00f);

        ImGui::Text("Malla: %d x %d (Celda = 0.25)", lienzo.columnas(), lienzo.filas());
        if (celdaCursor.esValida()) {
            ImGui::Text("Celda bajo cursor: (%d, %d)", celdaCursor.col, celdaCursor.fila);
        }
        else {
            ImGui::TextDisabled("Celda bajo cursor: (fuera)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(tonoAcento, "1. Primitivas y Herramientas");

        const char* nombresHerramientas[7] = {
            "Linea Bresenham",
            "Circulo Bresenham",
            "Formas Libres (Pincel)",
            "Triangulo en la Grilla (3 clics)",
            "Cuadrado en la Grilla",
            "Relleno de Primitivas (Bote)",
            "Borrador (Restaurar Rosa)"
        };

        int indiceHerramienta = static_cast<int>(herramientaActual);
        for (int i = 0; i < 7; ++i) {
            if (ImGui::RadioButton(nombresHerramientas[i], indiceHerramienta == i)) {
                herramientaActual = static_cast<Herramienta>(i);
                pasoTriangulo = 0;
                arrastrando = false;
            }
        }

        if (herramientaActual == Herramienta::Triangulo) {
            ImGui::TextColored(tonoAcento, "Vertices marcados: %d / 3", pasoTriangulo);
        }

        ImGui::Spacing();
        ImGui::Checkbox("Rellenar figura al trazar", &rellenarFigura);
        ImGui::ColorEdit4("Color de pincel", colorActual.data());

        ImGui::Spacing();
        if (ImGui::Button("Limpiar Malla")) {
            lienzo.limpiarTodo(COLOR_FONDO_ROSA);
            pasoTriangulo = 0;
            actualizarGPU = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Escena Inicial")) {
            cargarEscenaInicial();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Trazar por Coordenadas Exactas")) {
            ImGui::InputInt4("Linea (x0,y0,x1,y1)", datosLinea);
            if (ImGui::Button("Dibujar Linea")) {
                AlgoritmosDibujo::dibujarLinea(
                    lienzo,
                    { datosLinea[0], datosLinea[1] },
                    { datosLinea[2], datosLinea[3] },
                    colorActual
                );
                actualizarGPU = true;
            }

            ImGui::InputInt3("Circulo (xc,yc,r)", datosCirculo);
            if (ImGui::Button("Dibujar Circulo")) {
                AlgoritmosDibujo::dibujarCirculo(
                    lienzo,
                    { datosCirculo[0], datosCirculo[1] },
                    datosCirculo[2],
                    colorActual,
                    rellenarFigura
                );
                actualizarGPU = true;
            }

            ImGui::InputInt3("Cuadrado (x0,y0,lado)", datosCuadrado);
            if (ImGui::Button("Dibujar Cuadrado")) {
                const int desplazamiento = std::max(0, datosCuadrado[2] - 1);
                AlgoritmosDibujo::dibujarCuadrado(
                    lienzo,
                    { datosCuadrado[0], datosCuadrado[1] },
                    { datosCuadrado[0] + desplazamiento, datosCuadrado[1] + desplazamiento },
                    colorActual,
                    rellenarFigura
                );
                actualizarGPU = true;
            }

            ImGui::InputInt2("Tri V1 (x0,y0)", &datosTriangulo[0]);
            ImGui::InputInt2("Tri V2 (x1,y1)", &datosTriangulo[2]);
            ImGui::InputInt2("Tri V3 (x2,y2)", &datosTriangulo[4]);
            if (ImGui::Button("Dibujar Triangulo")) {
                AlgoritmosDibujo::dibujarTriangulo(
                    lienzo,
                    { datosTriangulo[0], datosTriangulo[1] },
                    { datosTriangulo[2], datosTriangulo[3] },
                    { datosTriangulo[4], datosTriangulo[5] },
                    colorActual,
                    rellenarFigura
                );
                actualizarGPU = true;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(tonoAcento, "2. Transformaciones 2D");
        ImGui::SliderFloat("Traslacion X", &moverX, -10.0f, 10.0f, "%.2f");
        ImGui::SliderFloat("Traslacion Y", &moverY, -10.0f, 10.0f, "%.2f");
        ImGui::SliderFloat("Rotacion (Grados)", &rotacionGrados, -180.0f, 180.0f, "%.1f");
        ImGui::SliderFloat("Escalamiento", &escalaZoom, 0.2f, 3.0f, "%.2f");

        ImGui::Spacing();
        if (ImGui::Button("Restaurar Transformaciones")) {
            moverX = 0.0f;
            moverY = 0.0f;
            rotacionGrados = 0.0f;
            escalaZoom = 1.0f;
        }

        ImGui::End();
    }

    void renderizarFotograma() {
        dibujarPanelImGui();

        const bool vistaPreviaArrastre = arrastrando && celdaCursor.esValida() &&
            (herramientaActual == Herramienta::Linea ||
                herramientaActual == Herramienta::Circulo ||
                herramientaActual == Herramienta::Cuadrado);

        const bool vistaPreviaTriangulo = (herramientaActual == Herramienta::Triangulo) && (pasoTriangulo > 0);

        if (actualizarGPU || vistaPreviaArrastre || vistaPreviaTriangulo) {
            actualizarGPU = false;

            // Copia temporal del lienzo para dibujar la vista previa sin dañar el original
            LienzoCuadricula lienzoTemporal = lienzo;

            if (vistaPreviaArrastre) {
                if (herramientaActual == Herramienta::Linea) {
                    AlgoritmosDibujo::dibujarLinea(lienzoTemporal, celdaInicio, celdaCursor, colorActual);
                }
                else if (herramientaActual == Herramienta::Circulo) {
                    const double dx = static_cast<double>(celdaCursor.col - celdaInicio.col);
                    const double dy = static_cast<double>(celdaCursor.fila - celdaInicio.fila);
                    const int radio = static_cast<int>(std::lround(std::hypot(dx, dy)));
                    AlgoritmosDibujo::dibujarCirculo(lienzoTemporal, celdaInicio, radio, colorActual, rellenarFigura);
                }
                else if (herramientaActual == Herramienta::Cuadrado) {
                    AlgoritmosDibujo::dibujarCuadrado(lienzoTemporal, celdaInicio, celdaCursor, colorActual, rellenarFigura);
                }
            }
            else if (vistaPreviaTriangulo) {
                for (int i = 0; i < pasoTriangulo; ++i) {
                    lienzoTemporal.pintarCelda(puntosTriangulo[i].col, puntosTriangulo[i].fila, colorActual);
                }
                if (pasoTriangulo == 2 && celdaCursor.esValida()) {
                    AlgoritmosDibujo::dibujarTriangulo(
                        lienzoTemporal,
                        puntosTriangulo[0],
                        puntosTriangulo[1],
                        celdaCursor,
                        colorActual,
                        rellenarFigura
                    );
                }
                else if (pasoTriangulo == 1 && celdaCursor.esValida()) {
                    AlgoritmosDibujo::dibujarLinea(lienzoTemporal, puntosTriangulo[0], celdaCursor, colorActual);
                }
            }

            dibujador.subirLienzo(lienzoTemporal, TAMANO_CELDA, mitadAnchoMundo, mitadAltoMundo);
        }

        const auto proyeccion = Matematicas2D::Matriz4::ortografica(
            -mitadAnchoMundo, mitadAnchoMundo, -mitadAltoMundo, mitadAltoMundo, -1.0f, 1.0f
        );
        const auto mvp = Matematicas2D::Matriz4::multiplicar(proyeccion, calcularMatrizModelo());

        // Fondo blanco para las líneas divisorias de la cuadrícula
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shaders.activarConMatriz(mvp);
        dibujador.dibujar();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(ventana);
    }

    // Eventos de ventana, ratón y teclado conectados con GLFW
    static void alCambiarTamano(GLFWwindow* win, int ancho, int alto) {
        if (ancho <= 0 || alto <= 0) return;
        glViewport(0, 0, ancho, alto);
        auto* app = static_cast<AplicacionPaint*>(glfwGetWindowUserPointer(win));
        if (!app) return;

        app->anchoVentana = ancho;
        app->altoVentana = alto;
        app->ajustarCuadricula();
    }

    static void alMoverRaton(GLFWwindow* win, double posX, double posY) {
        auto* app = static_cast<AplicacionPaint*>(glfwGetWindowUserPointer(win));
        if (!app) return;

        Celda celda{};
        const bool dentro = app->ratonACelda(posX, posY, celda);
        app->celdaCursor = dentro ? celda : Celda{ -1, -1 };

        if (ImGui::GetIO().WantCaptureMouse) return;

        if (app->arrastrando && dentro) {
            if (app->herramientaActual == Herramienta::PincelLibre) {
                AlgoritmosDibujo::dibujarLinea(app->lienzo, app->ultimaCeldaTrazo, celda, app->colorActual);
                app->ultimaCeldaTrazo = celda;
                app->actualizarGPU = true;
            }
            else if (app->herramientaActual == Herramienta::Borrador) {
                AlgoritmosDibujo::dibujarLinea(app->lienzo, app->ultimaCeldaTrazo, celda, COLOR_FONDO_ROSA);
                app->ultimaCeldaTrazo = celda;
                app->actualizarGPU = true;
            }
        }
    }

    static void alHacerClic(GLFWwindow* win, int boton, int accion, int /*mods*/) {
        if (ImGui::GetIO().WantCaptureMouse) return;

        auto* app = static_cast<AplicacionPaint*>(glfwGetWindowUserPointer(win));
        if (!app) return;

        double posX = 0.0, posY = 0.0;
        glfwGetCursorPos(win, &posX, &posY);

        Celda celda{};
        const bool dentro = app->ratonACelda(posX, posY, celda);

        if (boton == GLFW_MOUSE_BUTTON_LEFT && accion == GLFW_PRESS && dentro) {
            if (app->herramientaActual == Herramienta::BotePintura) {
                AlgoritmosDibujo::rellenarBoteBFS(app->lienzo, celda, app->colorActual);
                app->actualizarGPU = true;
            }
            else if (app->herramientaActual == Herramienta::Triangulo) {
                app->puntosTriangulo[app->pasoTriangulo] = celda;
                ++app->pasoTriangulo;
                if (app->pasoTriangulo == 3) {
                    AlgoritmosDibujo::dibujarTriangulo(
                        app->lienzo,
                        app->puntosTriangulo[0],
                        app->puntosTriangulo[1],
                        app->puntosTriangulo[2],
                        app->colorActual,
                        app->rellenarFigura
                    );
                    app->pasoTriangulo = 0;
                    app->actualizarGPU = true;
                }
            }
            else {
                app->arrastrando = true;
                app->celdaInicio = celda;
                app->ultimaCeldaTrazo = celda;

                if (app->herramientaActual == Herramienta::PincelLibre) {
                    app->lienzo.pintarCelda(celda.col, celda.fila, app->colorActual);
                    app->actualizarGPU = true;
                }
                else if (app->herramientaActual == Herramienta::Borrador) {
                    app->lienzo.pintarCelda(celda.col, celda.fila, COLOR_FONDO_ROSA);
                    app->actualizarGPU = true;
                }
            }
        }

        if (boton == GLFW_MOUSE_BUTTON_LEFT && accion == GLFW_RELEASE && app->arrastrando) {
            app->arrastrando = false;
            const Celda celdaFinal = dentro ? celda : app->celdaCursor;

            if (celdaFinal.esValida()) {
                if (app->herramientaActual == Herramienta::Linea) {
                    AlgoritmosDibujo::dibujarLinea(app->lienzo, app->celdaInicio, celdaFinal, app->colorActual);
                }
                else if (app->herramientaActual == Herramienta::Circulo) {
                    const double dx = static_cast<double>(celdaFinal.col - app->celdaInicio.col);
                    const double dy = static_cast<double>(celdaFinal.fila - app->celdaInicio.fila);
                    const int radio = static_cast<int>(std::lround(std::hypot(dx, dy)));
                    AlgoritmosDibujo::dibujarCirculo(app->lienzo, app->celdaInicio, radio, app->colorActual, app->rellenarFigura);
                }
                else if (app->herramientaActual == Herramienta::Cuadrado) {
                    AlgoritmosDibujo::dibujarCuadrado(app->lienzo, app->celdaInicio, celdaFinal, app->colorActual, app->rellenarFigura);
                }
                app->actualizarGPU = true;
            }
        }
    }

    static void alPresionarTecla(GLFWwindow* win, int tecla, int /*scancode*/, int accion, int /*mods*/) {
        if (ImGui::GetIO().WantCaptureKeyboard) return;
        if (accion != GLFW_PRESS && accion != GLFW_REPEAT) return;

        auto* app = static_cast<AplicacionPaint*>(glfwGetWindowUserPointer(win));
        if (!app) return;

        switch (tecla) {
        case GLFW_KEY_ESCAPE:
            if (app->pasoTriangulo > 0) app->pasoTriangulo = 0;
            else glfwSetWindowShouldClose(win, GLFW_TRUE);
            break;
        case GLFW_KEY_UP:    app->moverY += 0.25f; break;
        case GLFW_KEY_DOWN:  app->moverY -= 0.25f; break;
        case GLFW_KEY_LEFT:  app->moverX -= 0.25f; break;
        case GLFW_KEY_RIGHT: app->moverX += 0.25f; break;
        case GLFW_KEY_Q:     app->rotacionGrados += 5.0f; break;
        case GLFW_KEY_E:     app->rotacionGrados -= 5.0f; break;
        case GLFW_KEY_EQUAL:
        case GLFW_KEY_KP_ADD:
            app->escalaZoom += 0.05f;
            break;
        case GLFW_KEY_MINUS:
        case GLFW_KEY_KP_SUBTRACT:
            app->escalaZoom = std::max(0.1f, app->escalaZoom - 0.05f);
            break;
        case GLFW_KEY_0:
            app->moverX = 0.0f;
            app->moverY = 0.0f;
            app->rotacionGrados = 0.0f;
            app->escalaZoom = 1.0f;
            break;
        default:
            break;
        }
    }
};

int main() {
    AplicacionPaint app;
    return app.ejecutar();
} // Cierra main.