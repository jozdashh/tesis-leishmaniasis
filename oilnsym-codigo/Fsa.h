/*******************************************************************************************
fsa.h
Se implementa un arbol binario, que sirve para cargar en memoria un conjunto de muestras.

Autores: G. Alvarez
		 Jorge Hernan Victoria  (Mayo 27 de 2009)	
Fecha: abril 11 de 2005

Fecha de modificacion: feb 20 de 2006
   Se modifica aleatoriamente el etiquetamiento de los estados en el arbol aceptor de
   prefijos antes de empezar a ejecutar las comparaciones entre nodos.
Fecha de modificacion: Mayo 27 de 2009
  Se extiende el algoritmo OIL para soportar n-simbolos en el alfabeto de entrada.

*******************************************************************************************/
#ifndef _fsa_incluido
#define _fsa_incluido

#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include <queue>
#include <stack>
#include <string.h>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <utility>

using namespace std;

#define MAXLEN_SAMPLE 1000
#define SIZE_FILE_NAME  100

#define ORDEN_ALEATORIO    0
#define ORDEN_ASCENDENTE   1

class Edge
{
public:
   char firstMark;
   char lastMark;
};
class Tree;

/*******************************************************************************************
La clase nodeAuto contiene los elementos asociados con un nodo en el grafo, mas que una clase es un tipo, ya que por motivos de eficiencia se realizar�acceso directo a sus datos, no a traves de metodos.
Significado del campo marca:
'A' nodo activo
'B' nodo borrado
'C' arco recien eliminado pendiente de que se confirme la acción
'D' arco recien adicionado, pendiente de que se confirme la acción
*******************************************************************************************/
class NodeFsa
{
public:
   char sign;
   char lastSign;
   char mark;
   int alphabet;
   
   map<int,map<int,Edge> > suc;
   map<int,map<int,Edge> > pred;
      
   inline NodeFsa() 
   {
      mark = 'A';
      sign = '*';
      lastSign = '*';
      alphabet = 0;
      pred.clear();
      suc.clear();
   };
   NodeFsa(char s, char m, int alpha);
};

/*******************************************************************************************
La clase automata es la estructura de datos principal. Se inicializa con la informacion del
arbol binario y evoluciona mediantes agrupamientos de nodos hasta dar origen al automata
inferido.
Significado del campo marca:
'A' equivalencia activa
'B' equivalencia borrada
*******************************************************************************************/
class Fsa
{

public:
   int alphabet;
   list<int> initialStates;
   map<int,int> equivDone;
   map<int,char> markEquivDone;
   map<int,NodeFsa> graph;
   bool verbose;
   int initialStatesNumber;
   int lastInitialStatesNumber;
   inline Fsa() 
   { 
      initialStates.clear();
      equivDone.clear();
      markEquivDone.clear();
      graph.clear();
      verbose = false;
      initialStatesNumber = 0;
      lastInitialStatesNumber = 0;
   };
   void initAuto();
   int getNewRandomId(int infNewStates, int supNewStates, list<int> &identifiersUsed);
//    void Fsa::loadSamples(list<pair<char,char *> > posSamples);
   void loadOneSample(pair<char,vector<int> > posSample, int orden);
   void setPred(int idV1, int idV2, int symbol, char m);
   void setSuc(int idV1, int idV2, int symbol, char m);
   void setVertex(int idV, char m);
   void setEquiv(int idV1, int idV2, char m);
   void addVertex(int idV, char sign, char mark);
   void addEdge(int idV1, int idV2, int symbol, char mark);

   void mostrar (int id, NodeFsa n);
   void mostrarTodo();

   bool merge(int idV1, int idV2);
   list<int> getTotalListSuc(list<int> states, int symbol);
   bool buildNodeTrellis(char sampleSign, vector<int> sample);
   bool evalConsistency(list<pair<char, vector<int> > > negSamples);
   bool evalConsistencyOneSample(pair<char, vector<int> > oneSample);
   void redoMark(map<int,Edge> &list);
   void redoMerge();
   void undoMark(int idV, map<int,Edge> &list);
   void undoMerge();
   int getPos(list<int> translationTable, int idV);
   void translate(list<int> translationTable, map<int,map<int,Edge> > in,NodeFsa *tmpNode, int type);
   void makeHypothesis(Fsa &newF);
   void generateOutput(char outFile[]);

   int calcularPuntaje(list<pair<char, vector<int> > > posSamples);
   int estadoConMaxPuntaje(vector<pair<int,int> > puntajes);
   void mainProcess(list<pair<char, vector<int> > > negSamples, list<pair<char, vector<int> > > posSamples);

};

#endif
