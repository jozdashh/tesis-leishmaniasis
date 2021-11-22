# RPNI version 4.0
# Autor: Josue Peña Atencio
# Ultima modificación: 26 de Julio 2021

# Nota: los simbolos (letra asociada a cada gen) son expresados mediante numeros (1..n). Esto no tiene nada que ver con los índices de los estados del automata (enumerados 0..m), ya que estos representan sufijos. n != m.

from collections import deque
from copy import deepcopy
from utilities import write_automata, load_sample

# Estructura de datos para enumerar arbol de prefijos de moore
# Fuente: https://albertauyeung.github.io/2020/06/15/python-trie.html
class TrieNode:
    """A node in the trie structure"""
    def __init__(self, char):
        self.char = char
        self.children = {}

class Trie(object):
    """The trie object"""

    def __init__(self):
        """
        The trie has at least the root node.
        The root node does not store any character
        """
        self.root = TrieNode(tuple())
    
    def insert(self, word):
        """Insert a word into the trie"""
        node = self.root

        for char in word:
            if char in node.children:
                node = node.children[char]
            else:
                new_node = TrieNode(char)
                node.children[char] = new_node
                node = new_node

    def bfs(self):
        """Breadth-first traversal of the trie
        Args:
            - node: starting node.
            - prefix: the current prefix, for tracing a
                      word while traversing the trie.
        """
        Q = deque([(self.root, self.root.char)])
        ans = list()

        while len(Q)!=0:
            v, prefix = Q.popleft()
            ans.append(prefix)
            adj = list(v.children)
            adj.sort()
            for w in adj:
                Q.append((v.children[w], prefix+(v.children[w].char,)))

        return ans


def determinista(M):
    ans = True
    for i in range(len(M)):
        if M[i] != None:
            for s in M[i]['outs']:
                ans = ans and (len(M[i]['outs'][s]) == 1)
    return ans


def mezcla(M0, symbols, p, q):
    assert ((M0[p] != None) and (M0[q] != None))
    assert p < q
    if (M0[p]['val'] != '?') and (M0[q]['val'] != '?') and (M0[p]['val'] != M0[q]['val']):
        return M0
    if M0[p]['val'] == '?':
        M0[p]['val'] = M0[q]['val']

    for s in symbols:
        for r in range(len(M0)):
            if (M0[r] != None) and (s in M0[r]['outs']) and (q in M0[r]['outs'][s]):
                # Añadir transicion de r hacia p
                M0[r]['outs'][s].add(p)
                if s in M0[p]['ins']:
                    M0[p]['ins'][s].add(r)
                else:
                    M0[p]['ins'][s] = set([r])

                # Eliminar transición de r hacia q
                M0[r]['outs'][s].remove(q)
                M0[q]['ins'][s].remove(r)

    for s in symbols:
        for x in range(len(M0)):
            if (M0[x] != None) and (s in M0[x]['ins']) and (q in M0[x]['ins'][s]):
                # Añadir transicion de p hacia x
                if s in M0[p]['outs']:
                    M0[p]['outs'][s].add(x)
                else:
                    M0[p]['outs'][s] = set([x])
                M0[x]['ins'][s].add(p)

                # Eliminar transición de q hacia x
                M0[q]['outs'][s].remove(x)
                M0[x]['ins'][s].remove(q)
    
    # Eliminar el estado q. No hay transiciones saliendo o entrando a este estado.
    M0[q] = None
    return M0


# p y q son índices del grafo
def mezcladet(M, symbols, p, q):
    assert ((M[p] != None) and (M[q] != None))
    assert p < q
    M_ = deepcopy(M)
    lista = deque([(p, q)])
    while len(lista)!=0:
        r,s = lista.popleft()
        # hacer copias de los estados previas a la mezcla
        aux_r = deepcopy(M_[r]['outs'])
        aux_s = deepcopy(M_[s]['outs'])
        M1 = mezcla(deepcopy(M_),symbols,r,s)
        if M1 == M_:
            return M
        else:
            # Else: corregir el posible no-determinismo
            M_ = deepcopy(M1)
            for a in symbols:
                if (a in aux_r) and (a in aux_s):
                    aux = list()
                    aux.append(sorted(aux_r[a])[0])
                    aux.append(sorted(aux_s[a])[0])
                    if aux[0] == s: aux[0] = r
                    if aux[1] == s: aux[1] = r
                    aux.sort()
                    # caso especial: estados a mezclar son el mismo
                    if aux[0]!=aux[1]:
                        lista.append((aux[0],aux[1]))
    return M_


# Arbol de Prefjos de Moore
# Retorna el grafo correspondiente al arbol de prefijos de moore y el diccionario de prefijos a índices correspondiente
# X_p: muestras positivas
# X_n: muestras negativas
def APM(X_p, X_n, symbols):
    X = X_p.copy() + X_n.copy()
    trie = Trie()
    for word in X:
        trie.insert(word)
    index = dict() # mapa de prefijos a índices del grafo
    index2 = dict() # mapa de índices a prefijos del grafo
    prefixes = trie.bfs()
    for i in range(len(prefixes)):
        index[prefixes[i]] = i
        index2[i] = prefixes[i]
    G = [ dict(ins=dict(), outs=dict(), val='') for _ in range(len(prefixes)) ]
    X_p, X_n = set(X_p), set(X_n)
    for prefix in prefixes:
        for s in symbols:
            next = prefix+(s,)
            # Guardar la conexión siguiente a 'next', sólo si este ultimo pertenece al lenguaje de prefijos de X
            if next in index:
                G[index[prefix]]['outs'][s] = set([index[next]])
            # Guardar la conexión al estado previo (el primer prefijo, la cadena vacia, no tiene estado previo)
            if prefix != ():
                G[index[prefix]]['ins'][prefix[-1]] = set([index[prefix[:-1]]])
            # Asignar el valor de salida para el estado actual
            label = '?'
            if prefix in X_p:
                label = '1'
            elif prefix in X_n:
                label = '0'
            G[index[prefix]]['val'] = label
    return (G, index, index2)


# filename_input: nombre de archivo de palabras de entrada
# filename_output: nombre de archivo de salida para guardar automata resultante
def RPNI(filename_input, filename_output):
    alphabet_size, X, y = load_sample(filename_input)
    symbols = set()
    for w in X:
        for c in w:
            symbols.add(c)
    symbols = list(symbols)
    symbols.sort()
    # Agrupar palabras positivas y negativas
    X_p = list()
    X_n = list()
    for i in range(len(y)):
        if y[i] == 1:
            X_p.append(X[i])
        else:
            X_n.append(X[i])
    M, index, index2 = APM(X_p, X_n, symbols)
    # La primer posición de la lista representa el prefijo de la cadena vacía
    lista = [ _ for _ in range(len(index)) ]
    lista_ = lista[1:]
    lista_.sort(reverse=True)
    q = lista_[-1]
    while len(lista_)!=0:
        i = 0
        p = lista[i]
        while (i < len(lista)) and (p < q):
            M_ = mezcladet(deepcopy(M),symbols,p,q)
            assert determinista(M_)
            if M_ != M:
                M = deepcopy(M_)
                break
            i += 1
            p = lista[i]
        # Eliminar de lista los elementos que no están en M
        lista = [x for x in lista if M[x] != None]
        # Eliminar de lista_ los elementos que no están en M
        lista_ = [x for x in lista_ if M[x] != None]
        if len(lista_) > 0:
            q = lista_.pop()
    start_states = {0}
    end_states = set()
    M_aux = dict()
    for i in range(len(M)):
        # Quitar estados eliminados
        if M[i] != None:
            M_aux[i] = dict()
            for symb in M[i]['outs'].keys():
                M_aux[i][symb] = M[i]['outs'][symb]
            if M[i]['val'] == '1':
                end_states.add(i)
    M = deepcopy(M_aux)
    # Escribir automata resultante
    write_automata(filename_output, M, start_states, end_states)
