/*******************************************************************************************
Fsa.cpp
Se implementa un automata de estado finito, buscando rapidez en la ejecucion especialmente en las operaciones mas utilizadas para implementar el algoritmo rpni2.
Se implementan tambien las funciones encargadas de ejecutar los algoritmos de inferencia gramatical: rpni, rpni2

Uso: ./fsa muestrasEntrada automataSalida [Opciones]

Opciones:
-v (verbose) brinda información parcial durante la ejecución

Autor: G. Alvarez
Fecha: abril 17 de 2005

Fecha modificacion: marzo de 2006
Se incluye la modificacion del orden mediante una funcion asociada a cada cadena. Cada a vale 1 y cada b vale 2
el valor se asocia siguiendo las potencias de derecha a izquierda como si se tratara de pasar un valor binario a
decimal.

Fecha de modificacion: marzo 22 de 2006
Se  quitan las opciones aleatorias y se explora con varias opciones con diferentes tamaños de la tabla
que almacena la mezcla aleatoria y determina a partir de que tamaño se usa la funcion

Fecha de modificacion: abril 10 de 2006
Se modifica el procesamiento, que ahora se hace al estilo NRPNI4, se modifica el arbol inicial, que ahora es
no determinista y tiene un subautomata por cada muestra positiva. Los estados se permutan aleatoriamente, se
empiezan a realizar las mezclas en ese orden aleatorio, simplemente se agrupan, luego se pasan las muestras negativas si hay inconsistencia se deshace, sino se queda y paso a la siguiente mezcla.
CARACTERISTICAS DE ESTA VERSION: al cargar un incremento solo se sortean los nuevos estados, los anteriores
conservan sus identificadores ya asignados. Cuando el incremento no varia la hipotesis, esas muestras no
se incorporan para posterior uso

Fecha de modificacion: abril 25 de 2006
Cuando se considera un nuevo incremento en las muestras y estas son consistentes con la hipotesis actual, las nuevas
muestras positivas y negativas se agregan a las ya existentes para ser utilizadas en la evaluacion de consistencia.

Fecha de modificacion_ mayo 9 de 2006
Se corrige un error que habia al deshacer la union, ya que no se recuperaban correctamente los estados iniciales

Fecha de modificacion: mayo 22 de 2006
Se corrigen algunos delete y free que faltaban y causaban leak problems. Ahora el programa usa limpiamente la memoria
dinamica.

Fecha de modificacion: junio 20 de 2006
Se cambia el procesamiento principal para forzar la convergencia aun en caso que se adicionen nuevas muestras postivias que no son consistentes con la hipotesis actual. En ocasiones esta circunstancia causaba que se necesitaran
nuevas muestras negativas relacionadas con las positivas del incremento para lograr la convergencia.

Fecha de modificacion: octubre 3 de 2007
Se va a transformar en un algoritmo de inferencia para probarlo con los corpus normales. Para eso: se toman desde el principio todas las muestras negativas y las positivas se consideran en bloques de una sola muestra. El procesamiento continua como hasta ahora y se almacena la hipotesis resultante.

Fecha de modificacion: octubre 10 de 2007
Se adiciona un parametro de entrada para controlar el orden en el cual se asignan los identificadores al construir
el automata maximal: 0 orden aleatorio, 1 orden ascendente, 2 orden descendente

Fecha de modificacion: octubre 18 de 2007
Se va a introducir el puntaje para definir con quien se agrupa un estado si hay varios candidatos. Para eso se cuenta
cuantas muestras positivas sin procesar acepta el automata resultante en cada caso y se ejecuta la mezcla que maximice
el total
*******************************************************************************************/

#include "Fsa.h"
//#include "arBin.h"

// Prototipos de funciones auxiliares
int loadTrainSamples(char *archName, list<pair<char,vector<int> > > &negSamples, list<pair<char,vector<int> > > &posSamples);
bool cmp(vector<int> a,vector<int> b);
bool isShorter(pair<char, vector<int> > a, pair<char, vector<int> > b);
bool minKey(pair<int,NodeFsa> a, pair<int,NodeFsa>);


// *******************************************************************************************************
// FUNCIONES AUXILIARES QUE NO PERTENECEN A LA CLASE
// *******************************************************************************************************

int loadTrainSamples(char *archName, list<pair<char,vector<int> > > &negSamples, list<pair<char,vector<int> > > &posSamples)
{
   list<pair<char,char *> > ans;
   ifstream fd(archName);
   char line[MAXLEN_SAMPLE];
   char sampleSign;
   unsigned int init;
   char* symbol;
   vector<int> sampleV;
   vector<int> sampleV2;
   int symPos;
   int symInt;
   int alphabet;

   negSamples.clear();
   posSamples.clear();
   if ( !fd.is_open())
   {
      cerr << "Error abriendo el archivo de muestras" << endl;
      exit(1);
   }

  //almacena el tamano del alfabeto -----------
   fd.getline(line,MAXLEN_SAMPLE);
   
   init = 0;
   while((line[init] != ' ')&&(init<strlen(line))){
	   init++;
   }
   init++;
   symbol = (char *)malloc(MAXLEN_SAMPLE);
   memset(symbol,'\0',MAXLEN_SAMPLE);
   symPos = 0;
   while((line[init] != ' ')&&(init<strlen(line))){
	   symbol[symPos] = line[init];
	   symPos ++;
	   init++;
   }
   alphabet = atoi(symbol);
   
   //--------------------------------------------------------

   while ( !fd.eof())
   {
      memset(line,'\0',MAXLEN_SAMPLE);
      
      fd.getline(line, MAXLEN_SAMPLE);
      
      if (strlen(line) > 0)
      {
         sampleSign = line[0];
         init = 4;
         
         sampleV.clear();
         symbol = (char *)malloc(MAXLEN_SAMPLE);
         memset(symbol,'\0',MAXLEN_SAMPLE);
         symPos = 0;
         if(strlen(line)>3){
	         for(unsigned int i=init;i<strlen(line);i++){
	        	 while((line[i] != ' ')&&(i<strlen(line))){
	        		 symbol[symPos] = line[i];
	        		 symPos ++;
	        		 i++;
	        	 }
	        	 symInt = atoi(symbol);
	        	 sampleV.push_back(symInt);
	        	 //cout << symInt << endl;
	        	 symbol = (char *)malloc(MAXLEN_SAMPLE);
		         memset(symbol,'\0',MAXLEN_SAMPLE);
		         symPos = 0;
	        }
         }else{
        	 //cout << "VACIO";
         }
         //cout << "------------" << endl;
         
         if (sampleSign == '0')
        	negSamples.push_back(make_pair(sampleSign,sampleV));
        else
            posSamples.push_back(make_pair(sampleSign,sampleV));
      }
   }
   fd.close();
   return alphabet;
}

bool cmp(vector<int> a,vector<int> b){
	for(unsigned int i=0;i<a.size();i++){
		if(a[i] != b[i]){
			if(a[i] < b[i])
				return true;
			if(a[i] > b[i])
				return false;
		}
	}
	return false;
}

bool isShorter(pair<char, vector<int> > a, pair<char, vector<int> > b)
{
    if (a.second.size() < b.second.size())
      return true;
   else
      if (a.second.size() == b.second.size() && cmp(a.second,b.second))
         return true;
      else
         return false;
}


bool minKey(pair<int,NodeFsa> a, pair<int,NodeFsa> b)
{
   return (a.first < b.first);
}


// *******************************************************************************************************
// FUNCIONES DE CONSULTA POR PANTALLA
// *******************************************************************************************************

ostream& operator<< (ostream &ost, vector<int> v){
	for(vector<int>::iterator it = v.begin(); it != v.end(); ++it){
		ost << *it << " ";
	}
	return ost;
}

ostream& operator<< (ostream &ost, map<int,Edge> v){
	map<int,Edge>::const_iterator it;
	for(it = v.begin();it != v.end(); ++it)
		ost << it->first << " " << it->second.firstMark << " ";
	ost << endl;
	return ost;
}

ostream& operator<< (ostream &ost, NodeFsa n)
{
	map<int,Edge>::const_iterator it;
	ost << "sign:" << n.sign << "(" << n.lastSign << ") mark: " << n.mark;
	      
	for(int i=0;i<n.alphabet;i++){
		if(n.pred[i].size()>0){
	    	ost << " P" << i << ": ";
	    	for(it = n.pred[i].begin();it != n.pred[i].end(); ++it)
	    		 if ((it->second.firstMark == 'A' && it->second.lastMark == '*') ||
	    		     (it->second.lastMark == 'A'))
	    			ost << it->first << "(" << it->second.firstMark << ", " << it->second.lastMark << ") ";
		}
	}  
	for(int i=0;i<n.alphabet;i++){
		if(n.suc[i].size()>0){
	    	ost << " S" << i << ": ";
	    	for(it = n.suc[i].begin();it != n.suc[i].end(); ++it)
	    		 if ((it->second.firstMark == 'A' && it->second.lastMark == '*') ||
	    		     (it->second.lastMark == 'A'))
	    			ost << it->first << "(" << it->second.firstMark << ", " << it->second.lastMark << ") ";
		}
	}
	
	ost << endl;
	
    return ost;
}


void Fsa::mostrar (int id, NodeFsa n)
{
   map<int,Edge>::const_iterator it;

   if (n.mark == 'A')
   {
      cout << id <<":: sign:" << n.sign << "(" << n.lastSign << ") mark: " << n.mark;
      
      for(int i=0;i<alphabet;i++){
    	  if(n.pred[i].size()>0){
	    	  cout << " P" << i << ": ";
	    	  for(it = n.pred[i].begin();it != n.pred[i].end(); ++it)
	    		  if (it->second.firstMark == 'A')
	    			  cout << it->first << "(" << it->second.firstMark << ", " << it->second.lastMark << ") ";
    	  }
      }  
      for(int i=0;i<alphabet;i++){
    	  if(n.suc[i].size()>0){
	    	  cout << " S" << i << ": ";
	    	  for(it = n.suc[i].begin();it != n.suc[i].end(); ++it)
	    		  if (it->second.firstMark == 'A')
	    			  cout << it->first << "(" << it->second.firstMark << ", " << it->second.lastMark << ") ";
    	 }
      }
      cout << endl;
   }
}

void Fsa::mostrarTodo()
{
   map<int,NodeFsa>::const_iterator it;
   list<int>::const_iterator itL;
   
   cout << "mostrarTodo AUTOMATA:" << endl;
   cout << "initialStatesNumber: " << initialStatesNumber << endl;
   cout << "initial states: [";
   for (itL = initialStates.begin(); itL != initialStates.end(); ++itL)
      cout << *itL << " ";
   cout << "]" << endl << "finalStates: [";
   for(it = graph.begin();it != graph.end(); ++it)
   {
      if (it->second.mark == 'A' && it->second.sign == '1')
         cout << it->first << ", ";
   }
   cout << "]" << endl;
   for (it = graph.begin(); it != graph.end(); ++it)
      mostrar(it->first, it->second);
}


ostream& operator<< (ostream &ost, Fsa f)
{
   map<int,NodeFsa>::const_iterator it;
   list<int>::const_iterator itL;

   ost << "AUTOMATA:" << endl;
   ost << "initialStatesNumber: " << f.initialStatesNumber << endl;
   ost << "initial states: [";
   for (itL = f.initialStates.begin(); itL != f.initialStates.end(); ++itL)
      ost << *itL << " ";
   ost << "]" << endl << "finalStates: [";
   for(it = f.graph.begin();it != f.graph.end(); ++it)
   {
      if (it->second.mark == 'A' && it->second.sign == '1')
         ost << it->first << ", ";
   }
   ost << "]" << endl;
   for(it = f.graph.begin();it != f.graph.end(); ++it)
   {
      if (it->second.mark == 'A')
         ost << it->first << ": " << it->second;
   }
   return ost;
}


ostream& operator<< (ostream &ost, list<int> l)
{
   list<int>::const_iterator it;

   ost << "LISTA<int>: ";
   for(it = l.begin();it != l.end(); ++it)
   {
      ost << *it << " ";
   }
   ost << endl;
   return ost;
}


ostream& operator<< (ostream &ost, list<char> l)
{
   list<char>::const_iterator it;

   ost << "LISTA<char>: ";
   for(it = l.begin();it != l.end(); ++it)
   {
      ost << *it << " ";
   }
   ost << endl;
   return ost;
}


ostream& operator<< (ostream &ost, map<int,char> l)
{
   map<int,char>::const_iterator it;

   ost << "map<int,char>: ";
   for(it = l.begin();it != l.end(); ++it)
   {
      ost << "(" << it->first << ": " << it->second << ") ";
   }
   ost << endl;
   return ost;
}


ostream& operator<< (ostream &ost, list< pair<int,int> > m)
{
   list< pair<int,int> >::const_iterator it;

   ost << "list< pair<int,int> >: ";
   for(it = m.begin();it != m.end(); ++it)
   {
      ost << "[" << it->first << ", " << it->second << "] ";
   }
   ost << endl;
   return ost;
}


// *******************************************************************************************************
// CONSTRUCTORAS, SELECTORAS Y MODIFICADORAS DE LA CLASE Fsa
// *******************************************************************************************************

// Crea un nodo del automata inicializando sus atributos
NodeFsa::NodeFsa(char s, char m, int alpha)
{
   sign = s;
   lastSign = s;
   mark = m;
   pred.clear();
   suc.clear();
   alphabet = alpha;
}


void Fsa::initAuto()
{
   initialStates.clear();
   graph.clear();
   equivDone.clear();
   markEquivDone.clear();
   verbose = false;
   initialStatesNumber = 0;
}


int Fsa::getNewRandomId(int infNewStates, int supNewStates, list<int> &identifiersUsed)
{
   int csc;

   if (infNewStates == supNewStates)
   {
      identifiersUsed.push_back(infNewStates);
      return infNewStates;
   }

   do {
      if ((int)identifiersUsed.size() == supNewStates - infNewStates)
      {
         cout << "Error, hay menos valores en el rango de los que se necesitan" << endl;
         exit(1);
      }
      csc = infNewStates + (int) ((supNewStates - infNewStates) * 1.0 *rand()/(RAND_MAX + 1.0));
   } while (find(identifiersUsed.begin(),identifiersUsed.end(),csc) != identifiersUsed.end());
   identifiersUsed.push_back(csc);
   return csc;
}


void Fsa::loadOneSample(pair<char,vector<int> > posSample, int orden)
{
   list<int> identifiersUsed;
   int csc;
   int pos;
   int infNewStates, supNewStates;
   int lastState;
   
   identifiersUsed.clear();
   lastInitialStatesNumber = initialStatesNumber;
   infNewStates = initialStatesNumber;

   if (posSample.second.size() == 0)
      supNewStates = infNewStates + posSample.second.size()/2 + 1;
   else
      supNewStates = infNewStates + posSample.second.size() +  1;
      //cout << "sample ***" << posSample.second << "***" << endl; 
   	  //cout << "inf: " << infNewStates << " sup: " << supNewStates << endl;
   
   pos = 0;

   if (orden == ORDEN_ALEATORIO){
	  //cout << "PRINT**************** "<< orden << " " << infNewStates << " " << supNewStates << " " << identifiersUsed;
      csc = getNewRandomId(infNewStates,supNewStates,identifiersUsed);
      //cout << csc << endl;
   }
   else
	  csc = initialStatesNumber++;
      //cout << "csc: " << csc << endl;
   
   addVertex(csc,'*','A');
   initialStates.push_back(csc);
   lastState = csc;
   //mostrarTodo();
   while (pos < (int)posSample.second.size())
   {
      if (orden == ORDEN_ALEATORIO){
    	 //cout << "PRINT**************** "<< orden << " " << infNewStates << " " << supNewStates << " " << identifiersUsed;
         csc = getNewRandomId(infNewStates,supNewStates,identifiersUsed);
      }
      else
         csc = initialStatesNumber++;
         //cout << "csc: " << csc << endl;

      addVertex(csc,'*','A');
      addEdge(lastState,csc,posSample.second[pos],'A');
      lastState = csc;
      pos += 1;
   }
   graph[lastState].sign = '1';
   graph[lastState].lastSign = '1';

   if (orden == ORDEN_ALEATORIO)
      initialStatesNumber = supNewStates;
   
   //cout << "LoadOneSample" << endl;
   //mostrarTodo();
}


// Crea una relacion de predecesor: idV2 es predecesor de idV1 y estan conectados con el simbolo symbol
void Fsa::setPred(int idV1, int idV2, int symbol, char m)
{
   if ( graph[idV1].pred[symbol].count(idV2) == 0 || 
        (graph[idV1].pred[symbol].count(idV2) == 1 && 
         graph[idV1].pred[symbol][idV2].lastMark == '*' &&
         (graph[idV1].pred[symbol][idV2].firstMark == 'A' || graph[idV1].pred[symbol][idV2].firstMark == 'B')) )
   {
      graph[idV1].pred[symbol][idV2].firstMark = m;
      graph[idV1].pred[symbol][idV2].lastMark = '*';
   }
   else
      graph[idV1].pred[symbol][idV2].lastMark = m;
}


// Crea una relacion de sucesor: idV2 es sucesor de idV1 y estan conectados con el simbolo symbol
void Fsa::setSuc(int idV1, int idV2, int symbol, char m)
{
   if ( graph[idV1].suc[symbol].count(idV2) == 0 || 
        (graph[idV1].suc[symbol].count(idV2) == 1 && 
         graph[idV1].suc[symbol][idV2].lastMark == '*' &&
         (graph[idV1].suc[symbol][idV2].firstMark == 'A' || graph[idV1].suc[symbol][idV2].firstMark == 'B')) )
   {
      graph[idV1].suc[symbol][idV2].firstMark = m;
      graph[idV1].suc[symbol][idV2].lastMark = '*';
   }
   else
      graph[idV1].suc[symbol][idV2].lastMark = m;
}

/*Modifica la marca del vertice idV, asignándole el valor m.
Los valores posibles de la marca son:
'A' nodo activo
'B' nodo borrado
'C' nodo recien eliminado pendiente de confirmar la acción
'D' nodo recien creado, pendiente de confirmar la acción*/
void Fsa::setVertex(int idV, char m)
{
   graph[idV].mark = m;
}


/*Modifica la marca de la equivalencia entre los estados idV1 e idV2, asignándole el valor m.
Los valores posibles de la marca son:
'A' nodo activo
'B' nodo borrado
'C' nodo recien eliminado pendiente de confirmar la acción
'D' nodo recien creado, pendiente de confirmar la acción*/
void Fsa::setEquiv(int idV1, int idV2, char m)
{
   equivDone[idV1] = idV2;
   markEquivDone[idV1] = m;
}

// Adiciona un nuevo vertice al automata

void Fsa::addVertex(int idV, char sign, char mark)
{
   NodeFsa *tmp;

   tmp = new NodeFsa(sign,mark,alphabet);
   graph[idV] = *tmp;
   delete tmp;
}


// Adiciona un nuevo arco al automata
void Fsa::addEdge(int idV1, int idV2, int symbol, char mark)
{
	setPred(idV2,idV1,symbol,mark);
	setSuc(idV1,idV2,symbol,mark);
}

//******************************************************************************************************
// FUNCIONES DE MEZCLA DE ESTADOS
//******************************************************************************************************

// Mezcla dos estados, el resultado puede ser no determinista, se supone que id1 << id2 lexicograficamente
bool Fsa::merge(int idV1, int idV2)
{
   map<int,map<int,Edge> >::const_iterator it0;
   map<int,Edge>::const_iterator it,itX;

   if (graph[idV1].sign != '*' && graph[idV2].sign != '*' && graph[idV1].sign != graph[idV2].sign)
      return false;

   if (graph[idV1].sign != graph[idV2].sign && graph[idV2].sign != '*')
   {
      graph[idV1].lastSign = graph[idV1].sign;
      graph[idV1].sign = graph[idV2].sign;
   }

   int i;
   
   //for(int i=0;i<alphabet;i++){
   for(it0 = graph[idV2].pred.begin();it0 != graph[idV2].pred.end();it0++){
	   i = it0->first;
	   for (it = graph[idV2].pred[i].begin(); it != graph[idV2].pred[i].end(); ++it)
	   {
	      if ( (it->second.lastMark == '*' && it->second.firstMark != 'B' && it->second.firstMark != 'C') ||
	            (it->second.lastMark != '*' && it->second.lastMark != 'B' && it->second.lastMark != 'C') )        
	      {
	         for (itX = graph[it->first].suc[i].begin(); itX != graph[it->first].suc[i].end() && itX->first != idV1; ++itX);
	         if (itX == graph[it->first].suc[i].end() ||
	             (itX->second.lastMark == '*' && (itX->second.firstMark == 'B' || itX->second.firstMark == 'C')) ||
	             (itX->second.lastMark != '*' && (itX->second.lastMark == 'B' || itX->second.lastMark == 'C'))
	            )
	         {
	            setSuc(it->first,idV1,i,'D');
	            setPred(idV1,it->first,i,'D');
	         }
	         setSuc(it->first,idV2,i,'C');
	         setPred(idV2,it->first,i,'C');
	      }
	   }
   }
 
   //for(int i=0;i<alphabet;i++){
   for(it0 = graph[idV2].suc.begin();it0 != graph[idV2].suc.end();it0++){
   	   i = it0->first;
	   for (it = graph[idV2].suc[i].begin(); it != graph[idV2].suc[i].end(); ++it)
	   {
	      if ( (it->second.lastMark == '*' && it->second.firstMark != 'B' && it->second.firstMark != 'C') ||
	            (it->second.lastMark != '*' && it->second.lastMark != 'B' && it->second.lastMark != 'C') )        
	      {
	         for (itX = graph[it->first].pred[i].begin(); itX != graph[it->first].pred[i].end() && itX->first != idV1; ++itX);
	         if (itX == graph[it->first].pred[i].end() ||
	             (itX->second.lastMark == '*' && (itX->second.firstMark == 'B' || itX->second.firstMark == 'C')) ||
	             (itX->second.lastMark != '*' && (itX->second.lastMark == 'B' || itX->second.lastMark == 'C'))
	            )
	         {
	            setSuc(idV1,it->first,i,'D');
	            setPred(it->first,idV1,i,'D');
	         }
	         setSuc(idV2,it->first,i,'C');
	         setPred(it->first,idV2,i,'C');
	      }
	   }
   }
      
   if (find(initialStates.begin(),initialStates.end(),idV2) != initialStates.end())
   {
      initialStates.remove(idV2);
      if (find(initialStates.begin(),initialStates.end(),idV1) == initialStates.end())
         initialStates.push_back(idV1);
   }
   setVertex(idV2,'C');
   setEquiv(idV2,idV1,'D');
   return true;
}

// Crea la lista de sucesores, de una lista de estados.
list<int> Fsa::getTotalListSuc(list<int> states, int symbol)
{
   list<int> ans;
   list<int>::const_iterator itS;
   map<int,Edge>::const_iterator itIni,itEnd,itE;
   
   ans.clear();
   for (itS = states.begin(); itS != states.end(); ++itS)
   {	   
	   itIni = graph[*itS].suc[symbol].begin();
	   itEnd = graph[*itS].suc[symbol].end();
	   
      for (itE = itIni; itE != itEnd; ++itE)
      {            
         if ( (itE->second.lastMark == '*' && itE->second.firstMark != 'B' && itE->second.firstMark != 'C') ||
               (itE->second.lastMark != '*' && itE->second.lastMark != 'B' && itE->second.lastMark != 'C') )  
            if (find(ans.begin(),ans.end(),itE->first) == ans.end())
               ans.push_back(itE->first);
      }
   }
   return ans;
}


// Dada una muestra negativa, construye la tabla para detectar inconsistencias, inferir signos y agrupar
bool Fsa::buildNodeTrellis(char sampleSign, vector<int> sample)
{
   list<int> states;
   list<int>::const_iterator itS;
   unsigned int pos = 0;
   bool isPlus;
   
   states.clear();
   states = initialStates;
   
//    Recorre la muestra construyendo en cada iteracion una columna del trellis
   while (pos < sample.size() && !states.empty())
   {
      states = getTotalListSuc(states,sample[pos]);
      pos += 1;
   }
   
   isPlus = false;
   for (itS = states.begin(); itS != states.end(); ++itS)
   {
      if (graph[*itS].sign == '1'){
         isPlus = true;
         break;
      }
   }
   
   if (isPlus && sampleSign == '0')
      return false;
   if (!isPlus && sampleSign == '0')
      return true;
   if (isPlus && sampleSign == '1')
      return true;
//   if (!isPlus && sampleSign == '1')
      return false;
}


bool Fsa::evalConsistency(list<pair<char, vector<int> > > negSamples)
{
   list<pair<char, vector<int> > >::const_iterator itSample;
   
//    Construye el trellis para cada muestra, si detecta inconsistencia falla
   for (itSample = negSamples.begin(); itSample != negSamples.end(); ++itSample)
      if (!buildNodeTrellis(itSample->first,itSample->second))
      {
         if (verbose)
         {
            cout << "Fallo de consistencia en la muestra: ";
            cout << itSample->first << ": " << itSample->second << endl;      
         }
         return false;
      }
   return true;
}


bool Fsa::evalConsistencyOneSample(pair<char, vector<int> > oneSample)
{
   list<pair<char,vector<int> > > listOneSample;
   
   listOneSample.clear();
   listOneSample.push_back(oneSample);
   if (verbose){
	   cout << "listOneSample SIZE: " << listOneSample.size() << endl;
	   cout << "oneSample " << oneSample.second << endl;
   }
   return evalConsistency(listOneSample);
}


/*Ajusta las marcas en una lista que puede ser de sucesores o de predecesores 
 * en caso que la mezcla haya podido realizarse exitosamente. Todo lo que tenga 
 * marca 'C' queda eliminado, es decir, se le pone la marca 'B' y todo lo que 
 * tenga marca 'D' es adicionado, es
decir, se le asigna la marca 'A'*/
void Fsa::redoMark(map<int,Edge> &list)
{
   map<int,Edge>::iterator itM;

   for (itM = list.begin(); itM != list.end(); ++itM)
   {
      if (itM->second.lastMark != '*')
      {
         itM->second.firstMark = itM->second.lastMark;
         itM->second.lastMark = '*';
      }
      if (itM->second.firstMark == 'C')
      {
         itM->second.firstMark = 'B';
         itM->second.lastMark = '*';
      }
      else
      {
         if (itM->second.firstMark == 'D')
         {
            itM->second.firstMark = 'A';
            itM->second.lastMark = '*';
         }
      }
   }
}

/*Una vez se ha confirmado que la mezcla se pudo hacer exitosamente, se deben 
 * ajustar las marcas: todo lo que tenga marca 'C' queda eliminado, es decir, 
 * se le pone la marca 'B' y todo lo que tenga marca 'D' es adicionado, es
decir, se le asigna la marca 'A'*/
void Fsa::redoMerge()
{
   map<int,NodeFsa>::iterator it;
   map<int,char>::iterator itE;

   for (it = graph.begin(); it != graph.end(); ++it)
   {
      it->second.lastSign = it->second.sign;

      if (it->second.mark == 'C')
         it->second.mark = 'B';
      else
      {
         if (it->second.mark == 'D')
            it ->second.mark = 'A';
      }
      for(int i=0;i<alphabet;i++){
    	  redoMark(it->second.pred[i]);  
      }
      for(int i=0;i<alphabet;i++){
    	  redoMark(it->second.suc[i]);  
      }
   }
   for (itE = markEquivDone.begin(); itE != markEquivDone.end(); ++itE)
   {
      if (itE->second == 'C')
         itE->second = 'B';
      else
      {
         if (itE->second == 'D')
            itE->second = 'A';
      }
   }
}


/*Ajusta las marcas en una lista que puede ser de sucesores o de predecesores 
 * en caso que la mezcla no haya podido realizarse exitosamente y sea necesario 
 * deshacerla. Todo lo que tenga marca 'C' queda es recuperado, es decir, se le 
 * pone la marca 'A' y todo lo que tenga marca 'D' es eliminado, es decir, se le 
 * asigna la marca 'B'*/
void Fsa::undoMark(int idV, map<int,Edge> &list)
{
   map<int,Edge>::iterator itM;

   for (itM = list.begin(); itM != list.end(); ++itM)
   {
      itM->second.lastMark = '*';
      if (itM->second.firstMark == 'C')
         itM->second.firstMark = 'A';
      else
      {
         if (itM->second.firstMark == 'D')
            itM->second.firstMark = 'B';
      }
   }
}


/*Una vez que falla la mezcla, se debe recuperar el estado del grafo 
 * antes de comenzar la operación, para eso se deben restaurar las marcas.
 *  Todo lo que tenga marca 'C' queda es recuperado, es decir, se le pone 
 * la marca 'A' y todo lo que tenga marca 'D' es eliminado, es decir, se 
 * le asigna la marca 'B'*/
void Fsa::undoMerge()
{
   map<int,NodeFsa>::iterator it;
   map<int,char>::iterator itE;
   
   for (it = graph.begin(); it != graph.end(); ++it)
   {
      it->second.sign = it->second.lastSign;
      
      if (it->second.mark == 'C')
         it->second.mark = 'A';
      else
      {
         if (it->second.mark == 'D')
            it ->second.mark = 'B';
      }
      for(int i=0;i<alphabet;i++){
    	  undoMark(it->first,it->second.pred[i]);
      }
      for(int i=0;i<alphabet;i++){
          undoMark(it->first,it->second.suc[i]);
      }
      
   }
   for (itE = markEquivDone.begin(); itE != markEquivDone.end(); ++itE)
   {
      if (itE->second == 'C')
         itE->second = 'A';
      else
      {
         if (itE->second == 'D')
            itE->second = 'B';
      }
   }
}


/*Retorna la posicion de un identificador de estado en la tabla de traducción, 
 * esta posición será su nuevo identificador*/
int Fsa::getPos(list<int> translationTable, int idV)
{
   list<int>::const_iterator it;
   int pos=0;
   
   for (it = translationTable.begin(); it != translationTable.end(); ++it)
   {
      if (*it == idV)
         return pos;
      pos++;
   }
   return -1;
}


/*Renumera los estados del automata y actualiza todas las referencias a ellos, de 
 * manera que los estados queden representados con valores enteros consecutivos a partir de cero.*/
// type = 0 (predecesores), type = 1 (sucesores) 
void Fsa::translate(list<int> translationTable, map<int,map<int,Edge> > in, NodeFsa *tmpNode,int type)
{
   map<int,map<int,Edge> >::const_iterator it1;
   map<int,Edge>::const_iterator it2;
   int pos;
   
   for (it1 = in.begin(); it1 != in.end(); ++it1){
	   for (it2 = it1->second.begin(); it2 != it1->second.end(); ++it2){
	       if (it2->second.firstMark == 'A'){
	           pos = getPos(translationTable,it2->first);
	           if (pos != -1){
	        	   if(type == 0){
	        		   tmpNode->pred[it1->first][pos].firstMark = it2->second.firstMark;
		        	   tmpNode->pred[it1->first][pos].lastMark = it2->second.lastMark;
		           }else{
		        	   tmpNode->suc[it1->first][pos].firstMark = it2->second.firstMark;
		        	   tmpNode->suc[it1->first][pos].lastMark = it2->second.lastMark;
	        	   }
	           }
	           else
	              cerr << "Error: al traducir el estado " << it2->first << " no está en la tabla de traduccion" << endl;
	       }
	   }
   }
}


// Transforma el automata descartando todos los estados y transiciones que no tienen que ver exclusivamente con
// los estados que forman el conjunto s
void Fsa::makeHypothesis(Fsa &newF)
{
   map<int,NodeFsa>::const_iterator it;
   NodeFsa *tmp;
   list<int> translationTable;
   int pos= -1, idVert;
   list<int>::const_iterator itV;

// Construye tabla de traduccion de nombres de estados   
   translationTable.clear();
   for (it = graph.begin(); it != graph.end(); ++it)
   {
      if (it->second.mark == 'A')
         translationTable.push_back(it->first);
   }

// Traduce los estados iniciales
   for (itV = initialStates.begin(); itV != initialStates.end(); ++itV)
   {
      idVert = *itV;
      while (graph[idVert].mark != 'A' && graph[idVert].mark != 'D')
         idVert = equivDone[idVert];
      pos = getPos(translationTable,idVert);
      if (pos != -1)
      {
         if (find(newF.initialStates.begin(), newF.initialStates.end(), pos) == newF.initialStates.end())
            newF.initialStates.push_back(pos);
      }
      else
         cerr << "Error: al traducir el estado inicial " << idVert << " no está en la tabla de traduccion" << endl;
   }
   
// Copia a newF cambiando el nombre de los estados para que queden como enteros consecutivos a partir de 0
   for (it = graph.begin(); it != graph.end(); ++it){
	  if (it->second.mark == 'A'){
		 tmp = new NodeFsa();
         tmp->sign = it->second.sign;
         tmp->mark = 'A';
         tmp->alphabet = newF.alphabet;
         
         translate(translationTable,it->second.pred,tmp,0);
         translate(translationTable,it->second.suc,tmp,1);
         
         pos = getPos(translationTable,it->first);
         if (pos != -1)
            newF.graph[pos] = *tmp;
         else
            cerr << "Error: al traducir se hace mencion a un estado que no existe en la tabla de traduccion" << endl;
         delete tmp;
      }         
   }   
}   

// Almacena en un archivo texto el automata resultante
void Fsa::generateOutput(char outFile[])
{
   ofstream fd(outFile);
   map<int,NodeFsa>::const_iterator it;
   map<int,map<int,Edge> >::const_iterator it0;
   map<int,Edge>::const_iterator itS;
   list<int>::const_iterator itL;
   int sizeFsa = 0;
   int finalStatesNum = 0;
   int transNum = 0;

   if ( !fd.is_open())
      cerr << "Error abriendo el archivo de salida" << endl;

   fd << "# Alfabeto\n{";
   for(int i=0;i<alphabet;i++){
	   if(i == alphabet-1){
		   fd << i;
	   }else{
		   fd << i << ",";
	   }
   }
   fd << "}\n\n";
   fd << "# Numero de estados\n";
   for (it = graph.begin(); it != graph.end(); ++it)
   {
      if (it->second.mark == 'A')
      {         
         sizeFsa++;
         if (it->second.sign == '1')
            finalStatesNum++;
      }
   }
   fd << sizeFsa << "\n";
   fd << "# Estados iniciales\n";
   fd << initialStates.size() << " ";
   for (itL = initialStates.begin(); itL != initialStates.end(); ++itL)
      fd << *itL << " ";
   fd << "\n";
   
   fd << "# Estados finales\n";
   fd << finalStatesNum;
   for (it = graph.begin(); it != graph.end(); ++it)
      if (it->second.mark == 'A' && it->second.sign == '1')
         fd << " " << it->first;
   fd << " \n";
   
   fd << "# Descripcion de las transiciones\n";
   for (it = graph.begin(); it != graph.end(); ++it)
   {
      if (it->second.mark == 'A')
      {
    	 for(it0 = graph[it->first].suc.begin();it0 != graph[it->first].suc.end();it0++){
    	    for (itS = graph[it->first].suc[it0->first].begin(); itS != graph[it->first].suc[it0->first].end(); ++itS)
    		     if (itS->second.firstMark == 'A')
    		       transNum++;
    	 }
    	}
   }
   fd << transNum << "\n";
   for (it = graph.begin(); it != graph.end(); ++it)
   {
      if (it->second.mark == 'A')
      {
    	 for(it0 = graph[it->first].suc.begin();it0 != graph[it->first].suc.end();it0++){
    		 for (itS = graph[it->first].suc[it0->first].begin(); itS != graph[it->first].suc[it0->first].end(); ++itS){
    			 if (itS->second.firstMark == 'A'){
    				 fd << it->first << " " << it0->first << " " << itS->first << "\n";
    			 }
    		 }
    	 }
      }
   }
   fd.close();
}

int Fsa::calcularPuntaje(list<pair<char, vector<int> > > posSamples)
{
   list<pair<char, vector<int> > >::const_iterator itsamples;
  int acum = 0;

   for (itsamples = posSamples.begin(); itsamples != posSamples.end(); ++itsamples)
   {
      if (buildNodeTrellis(itsamples->first, itsamples->second))
         acum++;
   }
   return acum;
}


int Fsa::estadoConMaxPuntaje(vector<pair<int,int> > puntajes)
{
   vector<pair<int,int> >::const_iterator itP;
   int maximo = 0;
   int estado = -1;

   if (verbose)
      cout << "alternativas para elegir: ";
   for (itP = puntajes.begin(); itP != puntajes.end(); ++itP)
   {
      if (verbose)
         cout << "(" << itP->first << ", " << itP->second << ") ";
      if (itP->second > maximo)
      {
         maximo = itP->second;
         estado = itP->first;
      }
   }
   if (verbose)
      cout << endl;
   if (estado == -1)
      cerr << "Error grave, se busca el mayor puntaje cuando no hay candidatos" << endl;
   return estado;
}



void Fsa::mainProcess(list<pair<char, vector<int> > > negSamples, list<pair<char, vector<int> > > posSamples)
{
   char nameFile[SIZE_FILE_NAME];
   int i,j;
   bool initialStateAdded;
   bool initialStateDeleted;
   vector<pair<int,int> > puntajes; // <identificador,puntaje>
   int tmp;

   memset(nameFile,'\0',SIZE_FILE_NAME);
   //cout << "Main Process " <<  lastInitialStatesNumber << " " << initialStatesNumber << endl; 
   for (i = lastInitialStatesNumber; i < initialStatesNumber; ++i)
   {
      if (graph.count(i) != 0 && graph[i].mark == 'A')
      {
         if (verbose)
         {
            cout << "e: " << i << endl;
            cout << *this;
         }
         
         puntajes.clear();
         for (j = 0; j < initialStatesNumber && j < i; ++j)
         {
            if (graph.count(j) != 0 && graph[j].mark == 'A')
            {
               initialStateAdded = false;
               initialStateDeleted = false;
               if (find(initialStates.begin(),initialStates.end(),i) != initialStates.end())
               {
                  initialStateDeleted = true;
                  if (find(initialStates.begin(),initialStates.end(),j) == initialStates.end())
                     initialStateAdded = true;
               }

               if (merge(j,i))
                  if (evalConsistency(negSamples))
                  {
                     tmp = calcularPuntaje(posSamples);
                     puntajes.push_back(make_pair(j,tmp));
                  }

               undoMerge();
               if (initialStateDeleted)
                  initialStates.push_back(i);
               if (initialStateAdded)
                  initialStates.remove(j);
            }
         }
         if (puntajes.size() > 0)
         {
            j = estadoConMaxPuntaje(puntajes);
            initialStateAdded = false;
            initialStateDeleted = false;
            if (find(initialStates.begin(),initialStates.end(),i) != initialStates.end())
            {
               initialStateDeleted = true;
               if (find(initialStates.begin(),initialStates.end(),j) == initialStates.end())
                  initialStateAdded = true;
            }
            if (merge(j,i))
            {
               if (verbose)
                  cout << "Ha unido " << j << " con " << i << endl;
               redoMerge();
            }
            else
               cerr << "Error grave, fallo una mezcla que no podia fallar" << endl;
         }
      }
   }
}


/*Programa principal: carga las muestras en un arbol, 
 * inicializa el automata e implementa el ciclo principal del algoritmo rpni*/
int main(int argc, char *argv[])
{
   Fsa f,newF;
   char inFile[SIZE_FILE_NAME];
   char outFile[SIZE_FILE_NAME];
   list<pair<char, vector<int> > > negSamples, posSamples; // muestras negativas y postivias
   list<pair<char, vector<int> > >::iterator itsamples; // iterador sobre las muestras para liberar memoria
   bool verbose = false;
   bool posAnswer;
   unsigned int semilla;
   int orden;
   
   if (argc < 4 || argc > 5)
   {
      cerr << "Use: fsa inputFileName outputFileName orderIdenAsignement [-v]" << endl;
      cerr << "\t-v(verbose) It gives a trace of the execution" << endl;
      return 1;
   }
   memset(inFile,'\0',SIZE_FILE_NAME);
   memset(outFile,'\0',SIZE_FILE_NAME);
   strcpy(inFile,argv[1]);
   strcpy(outFile,argv[2]);
   orden = atoi(argv[3]);

   if (argc == 5 && strcmp(argv[4],"-v") == 0)
      verbose = true;
   if (verbose)
      f.verbose = true;
   
   semilla = time(NULL);
//    semilla = 1197030537;
//    cout << "semilla" << semilla << endl;
   srand(semilla);
   
   f.alphabet = loadTrainSamples(inFile,negSamples,posSamples);
   newF.alphabet = f.alphabet;
   negSamples.sort(isShorter);
   posSamples.sort(isShorter);   

   if (verbose)
   {
      cout << "Alfabeto: " << f.alphabet << endl;
	  cout << "Muestras positivas" << endl;
      for (itsamples = posSamples.begin(); itsamples != posSamples.end(); ++itsamples){
         cout << itsamples->second << endl;
      }
   
      cout << "Muestras negativas" << endl;
      for (itsamples = negSamples.begin(); itsamples != negSamples.end(); ++itsamples){
    	  cout << itsamples->second << endl;
      }
   }

   for (itsamples = posSamples.begin(); itsamples != posSamples.end(); ++itsamples)
   {
      if (verbose)
      {
         cout << "Empezando iteracion" << endl;
         cout << f;
      }
      posAnswer = f.evalConsistencyOneSample(*itsamples);
      if (!posAnswer)
      {
	   	 if (verbose)
	            cout << "va a adicionar la muestra: ***" << itsamples->second << "***" << endl;
         f.loadOneSample(*itsamples,orden);
         f.mainProcess(negSamples,posSamples);
	  }
   }
   
   if (verbose){
	   cout << f;
   	   cout << "makeHypothesis" << endl;
   }
   
   f.makeHypothesis(newF);
   if (verbose)
   {
      cout << endl <<  endl << "RESPUESTA" << endl;
      cout << newF;
   }
   newF.generateOutput(outFile);
   
   
   //Libera la memoria generada al almacenar las muestras de entrenamiento
   for (itsamples = negSamples.begin(); itsamples != negSamples.end(); ++itsamples)
      itsamples->second.clear();
   negSamples.clear();
   for (itsamples = posSamples.begin(); itsamples != posSamples.end(); ++itsamples)
      itsamples->second.clear();
   posSamples.clear();
   
   return 0;
}
