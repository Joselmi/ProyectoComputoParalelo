#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <cmath>
#include <cstdlib>
#include <omp.h>

using namespace std;

// Etiquetas que puede tener cada punto
const int RUIDO = 0;
const int COREPRIMERO = 1;
const int CORESEGUNDO = 2;

// Cuenta cuantos puntos hay en el archivo csv, uno por renglon
int contarPuntos(string ruta) {
    ifstream archivo(ruta);
    if (!archivo.is_open())
        return 0;

    int total = 0;
    double x, y;
    char coma;
    while (archivo >> x >> coma >> y)
        total++;

    archivo.close();
    return total;
}


// Lee las coordenadas x y de cada punto y las guarda en dos arreglos
void leerPuntos(string ruta, double* x, double* y, int n) {
    ifstream archivo(ruta);
    char coma;
    for (int i = 0; i < n; i++)
        archivo >> x[i] >> coma >> y[i];
    archivo.close();
}


// Lee la etiqueta de cada punto del csv que escribio el serial
bool leerEtiquetas(string ruta, int* etiqueta, int n) {
    ifstream archivo(ruta);
    if (!archivo.is_open())
        return false;

    double x, y;
    char coma1, coma2;
    for (int i = 0; i < n; i++)
        archivo >> x >> coma1 >> y >> coma2 >> etiqueta[i];
    archivo.close();
    return true;
}


// Escribe un csv con x, y y la etiqueta de cada punto
void escribirResultados(string ruta, double* x, double* y, int* etiqueta, int n) {
    ofstream archivo(ruta);
    archivo << fixed << setprecision(3);
    for (int i = 0; i < n; i++)
        archivo << x[i] << "," << y[i] << "," << etiqueta[i] << "\n";
    archivo.close();
}


// Regresa verdadero si los puntos i y j estan a distancia epsilon o menos
bool sonVecinos(double* x, double* y, int i, int j, double epsilon) {
    double distancia = sqrt((x[j] - x[i]) * (x[j] - x[i]) + (y[j] - y[i]) * (y[j] - y[i]));
    return distancia <= epsilon;
}


// Cuenta los vecinos del punto i entre las posiciones desde y hasta - 1
// Se deja de contar en cuanto junta los que faltan
int contarVecinos(double* x, double* y, int i, int desde, int hasta, double epsilon, int faltan) {
    int vecinos = 0;
    int j = desde;
    while (j < hasta && vecinos < faltan) {
        if (sonVecinos(x, y, i, j, epsilon))
            vecinos++;
        j++;
    }
    return vecinos;
}


// Regresa verdadero si entre las posiciones desde y hasta - 1 hay un core de
// primer orden a distancia epsilon del punto i
bool hayCoreCerca(double* x, double* y, int* etiqueta, int i, int desde, int hasta, double epsilon) {
    bool encontrado = false;
    int j = desde;
    // Se deja de buscar en cuanto encuentra un core de primer orden cercano
    while (j < hasta && !encontrado) {
        if (etiqueta[j] == COREPRIMERO && sonVecinos(x, y, i, j, epsilon))
            encontrado = true;
        j++;
    }
    return encontrado;
}


// Regresa verdadero si el punto i esta a distancia epsilon o menos del
// rectangulo de una particion, si no esta no hace falta revisarla
// El 1e-9 evita errores de redondeo
bool particionCerca(double x, double y, double izquierda, double derecha, double abajo, double arriba, double epsilon) {
    double dx = 0;
    double dy = 0;
    if (x < izquierda)
        dx = izquierda - x;
    else if (x > derecha)
        dx = x - derecha;
    if (y < abajo)
        dy = abajo - y;
    else if (y > arriba)
        dy = y - arriba;
    return sqrt(dx * dx + dy * dy) <= epsilon + 1e-9;
}


// Convierte k en una cuadricula de columnas por filas
// k = 2 es 2x1, k = 4 es 2x2, k = 8 es 4x2, k = 16 es 4x4, k = 32 es 8x4 y k = 64 es 8x8
void calcularCuadricula(int k, int& columnas, int& filas) {
    columnas = 1;
    filas = 1;
    while (columnas * filas < k) {
        if (columnas <= filas)
            columnas = columnas * 2;
        else
            filas = filas * 2;
    }
}


// Version paralela P2 del algoritmo ingenuo de DBSCAN para detectar ruido
// Divide los puntos en k particiones y cada hilo trabaja una particion completa
// Cada punto tambien revisa las particiones vecinas, porque un vecino a
// distancia epsilon puede estar del otro lado de la linea que las separa
void dbscanP2(double* x, double* y, int* etiqueta, int n, double epsilon, int minPuntos, int k) {
    int columnas = 0;
    int filas = 0;
    calcularCuadricula(k, columnas, filas);
    int numParticiones = columnas * filas;

    // Limites de los puntos para armar la cuadricula
    double minX = x[0];
    double maxX = x[0];
    double minY = y[0];
    double maxY = y[0];
    for (int i = 1; i < n; i++) {
        if (x[i] < minX)
            minX = x[i];
        if (x[i] > maxX)
            maxX = x[i];
        if (y[i] < minY)
            minY = y[i];
        if (y[i] > maxY)
            maxY = y[i];
    }
    double anchoX = (maxX - minX) / columnas;
    double anchoY = (maxY - minY) / filas;
    if (anchoX <= 0)
        anchoX = 1;
    if (anchoY <= 0)
        anchoY = 1;

    // Cuantas particiones hacia cada lado hay que revisar para cubrir epsilon
    // Si la particion es mas ancha que epsilon basta con una
    int alcanceX = (int)ceil(epsilon / anchoX);
    int alcanceY = (int)ceil(epsilon / anchoY);

    // Particion de cada punto y cuantos puntos tiene cada particion
    int* particion = new int[n];
    int* inicioParticion = new int[numParticiones + 1];
    for (int p = 0; p <= numParticiones; p++)
        inicioParticion[p] = 0;

    for (int i = 0; i < n; i++) {
        int cx = (int)((x[i] - minX) / anchoX);
        int cy = (int)((y[i] - minY) / anchoY);
        if (cx >= columnas)
            cx = columnas - 1;
        if (cy >= filas)
            cy = filas - 1;
        particion[i] = cy * columnas + cx;
        inicioParticion[particion[i] + 1]++;
    }

    // Posicion donde empieza cada particion
    for (int p = 0; p < numParticiones; p++)
        inicioParticion[p + 1] = inicioParticion[p + 1] + inicioParticion[p];

    // Se acomodan los puntos para que cada particion quede junta en memoria
    // orden[i] es el indice original del punto que quedo en la posicion i
    int* orden = new int[n];
    int* siguiente = new int[numParticiones];
    for (int p = 0; p < numParticiones; p++)
        siguiente[p] = inicioParticion[p];
    for (int i = 0; i < n; i++) {
        orden[siguiente[particion[i]]] = i;
        siguiente[particion[i]]++;
    }

    double* xOrden = new double[n];
    double* yOrden = new double[n];
    for (int i = 0; i < n; i++) {
        xOrden[i] = x[orden[i]];
        yOrden[i] = y[orden[i]];
    }

    int* etiquetaPaso1 = new int[n];
    int* etiquetaPaso2 = new int[n];

    // Paso 1
    // Un punto es core de primer orden si tiene al menos minPuntos vecinos
    // a distancia epsilon contandose a si mismo, en otro caso es ruido
    // Cada hilo toma una particion
    #pragma omp parallel for schedule(dynamic, 1)
    for (int p = 0; p < numParticiones; p++) {
        int cx = p % columnas;
        int cy = p / columnas;

        for (int i = inicioParticion[p]; i < inicioParticion[p + 1]; i++) {
            // Primero se cuentan los vecinos de su propia particion
            int vecinos = contarVecinos(xOrden, yOrden, i, inicioParticion[p], inicioParticion[p + 1], epsilon, minPuntos);

            // Si no alcanzan, se cuentan los de las particiones vecinas que esten cerca
            for (int vy = cy - alcanceY; vy <= cy + alcanceY && vecinos < minPuntos; vy++) {
                for (int vx = cx - alcanceX; vx <= cx + alcanceX && vecinos < minPuntos; vx++) {
                    // Solo particiones que existen en la cuadricula y que no son la propia
                    bool existe = vx >= 0 && vx < columnas && vy >= 0 && vy < filas;
                    bool propia = vx == cx && vy == cy;
                    if (existe && !propia) {
                        if (particionCerca(xOrden[i], yOrden[i], minX + vx * anchoX, minX + (vx + 1) * anchoX,
                                           minY + vy * anchoY, minY + (vy + 1) * anchoY, epsilon)) {
                            int q = vy * columnas + vx;
                            vecinos = vecinos + contarVecinos(xOrden, yOrden, i, inicioParticion[q], inicioParticion[q + 1],
                                                              epsilon, minPuntos - vecinos);
                        }
                    }
                }
            }

            if (vecinos >= minPuntos)
                etiquetaPaso1[i] = COREPRIMERO;
            else
                etiquetaPaso1[i] = RUIDO;
        }
    }
    // El for paralelo espera a que todos los hilos terminen antes del paso 2,
    // porque un ruido puede tener cerca un core de otra particion

    // Paso 2
    // Un punto ruido se vuelve core de segundo orden si esta a distancia
    // epsilon de algun core de primer orden
    // Se lee de etiquetaPaso1 y se escribe en etiquetaPaso2 para que ningun
    // hilo lea lo que escribe otro
    #pragma omp parallel for schedule(dynamic, 1)
    for (int p = 0; p < numParticiones; p++) {
        int cx = p % columnas;
        int cy = p / columnas;

        for (int i = inicioParticion[p]; i < inicioParticion[p + 1]; i++) {
            etiquetaPaso2[i] = etiquetaPaso1[i];
            if (etiquetaPaso1[i] == RUIDO) {
                // Primero se busca en su propia particion
                bool encontrado = hayCoreCerca(xOrden, yOrden, etiquetaPaso1, i, inicioParticion[p], inicioParticion[p + 1], epsilon);

                // Si no hay, se busca en las particiones vecinas que esten cerca
                for (int vy = cy - alcanceY; vy <= cy + alcanceY && !encontrado; vy++) {
                    for (int vx = cx - alcanceX; vx <= cx + alcanceX && !encontrado; vx++) {
                        // Solo particiones que existen en la cuadricula y que no son la propia
                        bool existe = vx >= 0 && vx < columnas && vy >= 0 && vy < filas;
                        bool propia = vx == cx && vy == cy;
                        if (existe && !propia) {
                            if (particionCerca(xOrden[i], yOrden[i], minX + vx * anchoX, minX + (vx + 1) * anchoX,
                                               minY + vy * anchoY, minY + (vy + 1) * anchoY, epsilon)) {
                                int q = vy * columnas + vx;
                                encontrado = hayCoreCerca(xOrden, yOrden, etiquetaPaso1, i, inicioParticion[q], inicioParticion[q + 1], epsilon);
                            }
                        }
                    }
                }

                if (encontrado)
                    etiquetaPaso2[i] = CORESEGUNDO;
            }
        }
    }

    // Se regresan las etiquetas al orden original de los puntos
    #pragma omp parallel for
    for (int i = 0; i < n; i++)
        etiqueta[orden[i]] = etiquetaPaso2[i];

    // Libera la memoria pedida
    delete[] particion;
    delete[] inicioParticion;
    delete[] orden;
    delete[] siguiente;
    delete[] xOrden;
    delete[] yOrden;
    delete[] etiquetaPaso1;
    delete[] etiquetaPaso2;
}


int main(int argc, char* argv[]) {
    // Letra de la computadora donde se corre, J o M, va al final de cada archivo
    if (argc < 2) {
        cout << "Falta la letra de la computadora, por ejemplo dbscanP2 J\n";
        return 1;
    }
    string equipo = argv[1];

    // Parametros del experimento tal como vienen en las instrucciones
    int tamanos[8] = {20000, 40000, 80000, 120000, 140000, 160000, 180000, 200000};
    int valoresK[6] = {2, 4, 8, 16, 32, 64};
    int repeticiones = 10;

    // Hilos: 1, cores virtuales entre 2, cores virtuales y cores virtuales por 2
    // Por defecto OpenMP crea un hilo por cada core virtual, el hilo 0 guarda cuantos son
    int cores = 0;
    int idHilo;
    #pragma omp parallel private(idHilo)
    {
        idHilo = omp_get_thread_num();
        if (idHilo == 0)
            cores = omp_get_num_threads();
    }
    int hilos[4] = {1, cores / 2, cores, cores * 2};

    // Mismos valores que usa el serial y la libreta
    double epsilon = 0.03;
    int minPuntos = 10;

    // Despues de la letra se puede dar un tamano y las repeticiones, por ejemplo dbscanP2 J 20000 1
    // Con un tamano solo se corre ese, se imprime en pantalla y no se escribe el archivo
    int tamanoUnico = 0;
    bool corridaCompleta = true;
    if (argc >= 3) {
        tamanoUnico = atoi(argv[2]);
        corridaCompleta = false;
    }
    if (argc >= 4)
        repeticiones = atoi(argv[3]);

    // Archivo con el tiempo promedio de cada configuracion
    ofstream archivoTiempos;
    if (corridaCompleta) {
        archivoTiempos.open("datos/tiempos_p2_" + equipo + ".csv");
        archivoTiempos << "puntos,hilos,k,promedio\n";
    }

    for (int t = 0; t < 8; t++) {
        // Con un tamano solo se corre ese
        if (corridaCompleta || tamanos[t] == tamanoUnico) {
            string rutaEntrada = "datos/" + to_string(tamanos[t]) + "_data.csv";
            string rutaSerial = "datos/" + to_string(tamanos[t]) + "_results_" + equipo + ".csv";

            int n = contarPuntos(rutaEntrada);
            if (n == 0) {
                cout << "No se pudo leer ningun punto de " << rutaEntrada << "\n";
                return 1;
            }

            // Memoria para las coordenadas y la etiqueta de cada punto
            double* x = new double[n];
            double* y = new double[n];
            int* etiqueta = new int[n];
            int* etiquetaSerial = new int[n];

            leerPuntos(rutaEntrada, x, y, n);

            // Etiquetas que escribio el serial, para comprobar que P2 da lo mismo
            if (!leerEtiquetas(rutaSerial, etiquetaSerial, n)) {
                cout << "No se pudo leer " << rutaSerial << "\n";
                return 1;
            }

            // Se escribe el csv con los puntos core y ruido, sin contar en el tiempo
            // Todas las configuraciones dan las mismas etiquetas, por eso basta una
            if (corridaCompleta) {
                dbscanP2(x, y, etiqueta, n, epsilon, minPuntos, 64);
                escribirResultados("datos/" + to_string(n) + "_results_p2_" + equipo + ".csv", x, y, etiqueta, n);
            }

            for (int a = 0; a < 6; a++) {
                int k = valoresK[a];
                for (int h = 0; h < 4; h++) {
                    omp_set_num_threads(hilos[h]);

                    // Corrida de calentamiento, tambien sirve para comparar con el serial
                    dbscanP2(x, y, etiqueta, n, epsilon, minPuntos, k);
                    int diferencias = 0;
                    for (int i = 0; i < n; i++)
                        if (etiqueta[i] != etiquetaSerial[i])
                            diferencias++;
                    if (diferencias > 0)
                        cout << "MISMATCH puntos " << n << " k " << k << " hilos " << hilos[h]
                             << " diferencias " << diferencias << "\n";

                    // Se corre el algoritmo varias veces y se promedia el tiempo
                    double suma = 0;
                    for (int r = 0; r < repeticiones; r++) {
                        double inicio = omp_get_wtime();
                        dbscanP2(x, y, etiqueta, n, epsilon, minPuntos, k);
                        double fin = omp_get_wtime();
                        suma = suma + (fin - inicio);
                    }
                    double promedio = suma / repeticiones;

                    if (corridaCompleta)
                        archivoTiempos << n << "," << hilos[h] << "," << k << "," << promedio << "\n";
                    else
                        cout << "puntos " << n << " k " << k << " hilos " << hilos[h]
                             << " promedio " << promedio << " s\n";
                }
            }
            cout << n << " puntos listo\n";

            // Libera la memoria pedida
            delete[] x;
            delete[] y;
            delete[] etiqueta;
            delete[] etiquetaSerial;
        }
    }

    if (corridaCompleta)
        archivoTiempos.close();
    return 0;
}
