Juliana Andrea Torres Medina
6014466
# Proyecto Paint 2D

Aplicación interactiva de graficación por computadora y rasterización 2D desarrollada en **C++** con **OpenGL 3.3 Core**, **GLAD**, **GLFW** y **Dear ImGui**. Permite dibujar primitivas gráficas sobre una cuadrícula lógica discreta, rellenar áreas cerradas y aplicar transformaciones geométricas 2D en tiempo real manteniendo la precisión de interacción con el ratón.

## Descripción del Proyecto

El sistema simula un lienzo matricial dividido en celdas discretas donde cada primitiva se rasteriza mediante algoritmos clásicos de computación gráfica antes de ser renderizada en la tarjeta gráfica (GPU). 

### Características Principales
* **Primitivas Rasterizadas:**
  * **Línea de Bresenham (`dibujarLinea`):** Trazado mediante aritmética entera (algoritmo del punto medio) separado por pendientes suaves ($\vert{}\Delta x\vert{} \ge \vert{}\Delta y\vert{}$) y fuertes ($\vert{}\Delta y\vert{} > \vert{}\Delta x\vert{}$).
  * **Círculo de Bresenham (`dibujarCirculo`):** Cálculo entero basado en el parámetro de decisión $d = 3 - 2r$ aprovechando la simetría de 8 octantes, con soporte para contorno o relleno por franjas horizontales.
  * **Formas Libres (`Herramienta::PincelLibre`):** Trazo continuo a mano alzada conectando celdas consecutivas mediante `dibujarLinea` para evitar discontinuidades al mover rápido el cursor.
  * **Triángulos en la Grilla (`dibujarTriangulo`):** Selección interactiva de 3 vértices en pantalla y rasterización interior mediante evaluación de producto cruzado dentro de su caja delimitadora (*bounding box*), compatible con orientación horaria y antihoraria.
  * **Cuadrados (`dibujarCuadrado`):** Construcción simétrica tomando el lado máximo entre desplazamiento horizontal y vertical, con opción de relleno o perímetro.
  * **Relleno de Primitivas (`rellenarBoteBFS`):** Herramienta de bote de pintura (*Flood Fill*) implementada mediante Búsqueda en Anchura (**BFS**) con cola FIFO y matriz de celdas visitadas sobre los 4 vecinos ortogonales.
  * **Borrador:** Restaura las celdas seleccionadas al color base rosado pastel de la grilla.
* **Transformaciones Geométricas 2D en Tiempo Real:**
  * Traslación en ejes $X$ e $Y$, rotación en grados y escalamiento (*zoom*) ejecutados en GPU mediante matrices $4 \times 4$ (`uMatrizMVP`).
  * **Inversión de Matriz Afín (`invertirAfin2D`):** Transforma las coordenadas de pantalla del cursor de vuelta al espacio lógico de la grilla, permitiendo seguir dibujando con exactitud incluso cuando el tablero está desplazado, rotado o escalado.
* **Interfaz Gráfica y Control:**
  * Panel flotante en **Dear ImGui** con selector de herramientas, selector de color RGBA, modo de relleno, ingreso de primitivas por coordenadas exactas, *sliders* de transformación y atajos de teclado mediante **GLFW**.
## Paradigma de Programación

El proyecto aplica un enfoque **Multiparadigma**, combinando cuatro estilos complementarios según la responsabilidad de cada módulo:

1. **Programación Orientada a Objetos (POO) con RAII:**
   * Se encapsulan el estado y el ciclo de vida de los recursos en clases especializadas (`LienzoCuadricula`, `GestorShaders`, `DibujadorMalla` y `AplicacionPaint`).
   * Se emplea el principio **RAII** (*Resource Acquisition Is Initialization*) en los destructores de `GestorShaders` y `DibujadorMalla` para liberar automáticamente shaders y buffers (`VAO`/`VBO`) de OpenGL al cerrar la aplicación.
2. **Programación Modular / Procedural mediante Espacios de Nombres (`namespaces`):**
   * Las operaciones matemáticas (`Matematicas2D`) y los algoritmos de rasterización (`AlgoritmosDibujo`) se agrupan como funciones puras y desacopladas que operan directamente sobre estructuras de datos (`Matriz4`, `LienzoCuadricula`, `Celda`) sin depender de la ventana ni de OpenGL.
3. **Programación Dirigida por Eventos (*Event-Driven Programming*):**
   * La interacción del usuario se gestiona de forma asíncrona mediante *callbacks* de **GLFW** (`alCambiarTamano`, `alMoverRaton`, `alHacerClic` y `alPresionarTecla`).
4. **Bucle en Tiempo Real e Interfaz en Modo Inmediato (*Immediate Mode GUI*):**
   * El ciclo principal (`while (!glfwWindowShouldClose(ventana))`) actualiza el estado lógico, sincroniza los buffers de GPU solo cuando hay cambios (`actualizarGPU`) y reconstruye la interfaz de **Dear ImGui** en cada fotograma.

## Arquitectura y Estructura del Código

### Organización del Repositorio
```text
GrillaAcademica-Paint2D/
├── Main.cpp               # Código fuente completo organizado en capas lógicas
├── include/               # Cabeceras de dependencias (GLAD, GLFW, Dear ImGui)
├── lib/                   # Bibliotecas enlazadas (glfw3.lib, opengl32.lib)
└── README.md              # Documentación técnica y arquitectónica del proyecto
```

### Arquitectura Interna de `Main.cpp` (Separación de Responsabilidades - Estilo MVC)

El archivo `Main.cpp` está estructurado en **4 capas lógicas** claramente delimitadas:

| Capa | Componentes en `Main.cpp` | Responsabilidad |
| :--- | :--- | :--- |
| **1. Modelo y Matemáticas** | `Color`, `Celda`, `Herramienta`, `namespace Matematicas2D` (`Punto2D`, `Matriz4`), `class LienzoCuadricula` | Define los tipos de datos, almacena la matriz bidimensional de colores de la grilla, valida límites y calcula matrices de transformación $4 \times 4$ y su inversa. |
| **2. Lógica de Dominio (Algoritmos)** | `namespace AlgoritmosDibujo` (`dibujarLinea`, `dibujarCirculo`, `dibujarCuadrado`, `dibujarTriangulo`, `rellenarBoteBFS`) | Implementa los algoritmos discretos de rasterización y relleno de forma totalmente independiente a la tarjeta gráfica. |
| **3. Vista / Renderizado GPU** | `class GestorShaders`, `class DibujadorMalla` | Compila y enlaza shaders GLSL 3.30, envía la matriz `uMatrizMVP` y convierte cada celda del lienzo en 2 triángulos (6 vértices con margen del 5% por lado) dentro de un `VAO` y dos `VBO` (posiciones y colores). |
| **4. Controlador e Interfaz** | `class AplicacionPaint`, `int main()` | Inicializa GLFW/GLAD/ImGui, administra el panel de herramientas, captura eventos de ratón/teclado y coordina la actualización entre el modelo y la GPU. |

## Controles y Atajos de Teclado

| Control / Tecla | Acción |
| :--- | :--- |
| **Clic Izquierdo (Arrastrar)** | Dibuja con la herramienta activa (Línea, Círculo, Pincel libre, Cuadrado o Borrador). |
| **Clic Izquierdo (3 pasos)** | Marca los 3 vértices consecutivos cuando está activa la herramienta **Triángulo**. |
| **`ESC`** | Cancela la selección de vértices en curso al dibujar un triángulo. |
| **Flechas (`↑`, `↓`, `←`, `→`)** | Traslada la cuadrícula en los ejes $X$ e $Y$. |
| **`+` / `-`** | Aumenta o disminuye el nivel de *zoom* (escalamiento). |
| **`R`** | Reinicia todas las transformaciones 2D a sus valores por defecto. |
