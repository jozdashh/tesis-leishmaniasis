# Conjunto de funciones e utilidades para el procesamiento de datos
# Autor: Josue Peña Atencio
# Ultima modificación: 26 de Julio 2021

from pandas import concat, DataFrame
from copy import deepcopy

# Retornar las primeras y ultimas 'cant' filas de un dataframe 'df'
def filter_rows(df, cant):
    data_frames = [df.iloc[0:cant], df.iloc[len(df)-cant:len(df)]]
    return concat(data_frames)
    
# Aplanar el contenido de diccionarios que contienen
# información pre-procesada de pacientes en conjuntos de fácil acceso X, y
# Dp: datos de pacientes que no curaron
# Dn: datos de pacientes que curaron
def flatten(Dp, Dn):
    X = list()
    y = list()
    D = {**Dp, **Dn}
    for ID in D.keys():
        for cita in D[ID]:
            X.append(D[ID][cita])
            if ID in Dp:
                y.append(1)
            elif ID in Dn:
                y.append(0)
            else:
                raise Exception("Paciente con resultado de tratamiento inconcluso")
    return X, y

# filename: nombre de archivo en el cual guardar el automata
# M: automata (diccionario)
# start_states: conjunto de estados iniciales
# end_states: conjunto de estados finales o de aceptación
def write_automata(filename, M, start_states, end_states):
    alphabet = set()
    for state in M:
        for symb in M[state]:
            alphabet.add(symb)
    with open(filename,'w') as tf:
        tf.write("# Alfabeto\n")
        tf.write("{" + ",".join(map(str,sorted(alphabet))) + "}\n")
        tf.write("\n")
        tf.write("# Numero de estados\n")
        tf.write(f"{len(M)}\n")
        tf.write("# Estados iniciales\n")
        tf.write(f"{len(start_states)} ")
        tf.write(" ".join(map(str,sorted(start_states))) + "\n")
        tf.write("# Estados finales\n")
        tf.write(f"{len(end_states)} " + " ".join(map(str,sorted(end_states))) + "\n")
        tf.write("# Descripcion de las transiciones\n")
        n_trans = 0 # Cantidad de transiciones
        for s in M: n_trans += len(M[s])
        tf.write(f"{n_trans}\n")
        for state_1 in sorted(M.keys()):
            for symbol in sorted(M[state_1].keys()):
                for state_2 in sorted(M[state_1][symbol]):
                    tf.write(f"{state_1} {symbol} {state_2}\n")


# filename: nombre de archivo del automata a cargar
def load_automata(filename):
    with open(filename,'r') as tf:
        results = list(tf)
    #alphabet = set(map(int,results[1].strip()[1:-1].split(',')[1:]))
    start_states = set(map(int,results[6].strip().split()[1:]))
    end_states = set(map(int,results[8].strip().split()[1:]))
    M = {}
    # Caso borde: incluso si no hay ninguna transicion, el diccionario del automata queda bien formado
    for s in start_states:
        M[s] = dict()
    for s in end_states:
        M[s] = dict()
    # Añadir transiciones
    for i in range(11,len(results)):
        v,s,w = map(int,results[i].strip().split())
        if v not in M:
            M[v] = {s:set([w])}
        elif s not in M[v]:
            M[v][s] = set([w])
        else:
            M[v][s].add(w)
        if w not in M:
            M[w] = dict()
    return M, start_states, end_states
    

# filename: nombre de archivo al cual escribir la muestra
# alphabet_size: tamaño del alfabeto de símbolos de la muestra
# X: palabras de pacientes
# y: etiqueta de cada palabra (1 ó 0)
def write_sample(filename, alphabet_size, X, y):
    with open(filename,'w') as tf:
        # Cantidad de muestras y cantidad de símbolos
        tf.write(f"{len(X)} {alphabet_size}\n")
        for i in range(len(X)):
            tf.write(f"{y[i]} {len(X[i])} ")
            for s in X[i]:
                tf.write(f"{s} ")
            tf.write("\n")


# filename: nombre del archivo de muestra a cargar
def load_sample(filename):
    alphabet = set()
    X = list()
    y = list()
    with open(filename,'r') as tf:
        results = list(tf)
    for w in results[1:]:
        word = tuple(map(int,w[3:].split()))
        X.append(word)
        for c in word:
            alphabet.add(c)
        if w[0] == '1':
            y.append(1)
        elif w[0] == '0':
            y.append(0)
        else:
            raise Exception("Etiqueta de palabra incorrecta")
    return alphabet, X, y


# M: automata (diccionario) estado -> simbolo -> estado(s)
# start_states: estados iniciales
# end_states: estados finales (de aceptación)
# X: lista de palabras a probar en en el automata
def predict(M, start_states, end_states, X):
    pred = list()
    for word in X:
        states_curr = start_states.copy()
        states_next = set()
        incomplete = False
        for symbol in word:
            for state in sorted(states_curr):
                if symbol in M[state]:
                    states_next.update(M[state][symbol])
            # No se pudo hacer ninguna trancisión a partir del símbolo actual
            # y no se ha terminado de consumir la cadena
            if (len(states_next) == 0):
                incomplete = True
                break
            states_curr = states_next.copy()
            states_next = set()
        if not incomplete:
            if len(states_curr & end_states) > 0:
                # Aceptar la cadena
                pred.append(1)
            else:
                # Rechazar la cadena
                pred.append(0)
        else:
            pred.append(0)
    return pred


# Preparar datos para técnicas
# Crear alfabeto y palabras de cada paciente para cada cita

# x_patients: datos de expresión génica de pacientes de célula 'x' (neutrofilos, eosinofilos o monocitos)
# x_genes: datos de genes célula 'x' de pacientes
# outcomes: resultados de tratamiento para cada paciente

# cant: cantidad de genes más sobre expresados e infra expresados a seleccionar, respectivamente
# (total de genes seleccionados = cant*2)

# cod: tipo de codificación de genes:
# 'binary' (genes sobre e infra expresados) o 'ternary' (sobre, infra y genes estables)

# Retorna dos diccionarios: Dp (muestras positivas), Dn (muestras negativas)
# Estructura: ID_de_paciente --> {C1 : palabra, C2 : palabra, C3 : palabra} (Cada CX es una cita)
# No todos los pacientes tienen las 3 citas

def pre_process(x_patients,x_genes,outcomes,cant,cod):
    conflicts = dict()
    fraction = (1/4)
    # Obtener los génes más sobre e infra expresados.
    x_genes_aux = filter_rows(x_genes, cant) # x_genes entra ordenado descendientemente según 'deseq_logfc'
    x_patients_aux = x_patients.loc[x_patients.index.intersection(x_genes_aux.index)]
    x_describe = x_patients_aux.apply(DataFrame.describe, axis=1)
    Dp = dict() # Palabras positivas
    Dn = dict() # Palabras negativas
    # Construir palabras para cada paciente
    for p_ID in x_patients_aux:
        word = tuple()
        # sector = n : cantidad total de genes usados
        # Números del 1..n son genes infra exp; n+1..2n son genes sobre exp; 2n+1..3n son genes estables
        # Si la codificación es binaria el tamaño del alfabeto es 2n
        # Si es ternaria el tamaño del alfabeto es 3n
        sector = cant*2
        i = 1
        gen_info = list()
        for gen in x_patients_aux.index:
            mRNA_amount = x_patients_aux[p_ID].loc[gen]
            mean = x_describe.loc[gen]['mean']
            std = x_describe.loc[gen]['std']

            dist_inf = abs(mRNA_amount-(mean - std)) # Distancia de valor del gen a clasificación infra-expresado
            dist_sob = abs(mRNA_amount-(mean + std)) # Distancia a sobre-expresado
            dist_est_1 = abs(mRNA_amount-(mean - fraction*std)) # Distancia a borde inferior estable
            dist_est_2 = abs(mRNA_amount-(mean + fraction*std)) # Distancia a borde superior estable
            if cod=='binary':
                gen_info.append([i, dist_inf, dist_sob])
            else:
                gen_info.append([i, dist_inf, dist_sob, dist_est_1, dist_est_2])

            # Genes infra exp
            if mRNA_amount <= (mean - std):
                word += (i,)
            # Genes sobre exp
            elif mRNA_amount >= (mean + std):
                word += (sector+i,)
                gen_info[-1][0] += sector
            # Genes estables
            elif cod=='ternary' and ((mean - fraction*std) <= mRNA_amount <= (mean + fraction*std)):
                word += ((2*sector)+i,)
                gen_info[-1][0] += (2*sector)
            i += 1
        
        # Valor de salida del paciente p_ID (0.0 cura, 1.0 falla)
        outcome = outcomes.loc['SU'+p_ID[1:-2]]['EF_LC_ESTADO_FINAL_ESTUDIO']
        
        # Si la palabra queda vacía
        if len(word) == 0:
            # Gen más cercano a ser clasificado
            total_min_1 = min(gen_info, key=lambda x : min(x[1:]))
            gen_info.remove(total_min_1)
            # Segundo mejor gen
            # Con 2 genes es suficiente para diferenciar las palabras creadas mediante este método
            # sin agregar muchos símbolos
            total_min_2 = min(gen_info, key=lambda x : min(x[1:]))
            gen_info.remove(total_min_2)
            
            # Redondear distancias
            total_min_1 = list(map(lambda x: round(x,3), total_min_1))
            total_min_2 = list(map(lambda x: round(x,3), total_min_2))
            
            index_1 = total_min_1.index(min(total_min_1))
            index_2 = total_min_2.index(min(total_min_2))
            symbol_1, symbol_2 = None, None
            
            # Generar símbolos
            if index_1 == 1: # Infra-expresado
                symbol_1 = total_min_1[0]
            elif index_1 == 2: # Sobre-expresado
                symbol_1 = 2*total_min_1[0]
            elif index_1 in [3,4]: # Estable
                symbol_1 = 3*total_min_1[0]
            
            if index_2 == 1:
                symbol_2 = total_min_2[0]
            elif index_2 == 2:
                symbol_2 = 2*total_min_2[0]
            elif index_2 in [3,4]:
                symbol_2 = 3*total_min_2[0]
                
            word = (symbol_1,symbol_2)
            
        if word not in conflicts:
            conflicts[word] = (0, outcome) # (cantidad de conflictos, última etiqueta detectada)
        else:
            if outcome != conflicts[word][1]:
                conflicts[word] = (conflicts[word][0]+1,outcome)
                
        # Clasificar palabra como positiva o negativa
        normal_ID = p_ID[:-2]
        cita = 'C'+p_ID[-1:]
        if outcome == 1.0:
            if normal_ID not in Dp:
                Dp[normal_ID] = dict()
            Dp[normal_ID][cita] = word
        elif outcome == 0.0:
            if normal_ID not in Dn:
                Dn[normal_ID] = dict()
            Dn[normal_ID][cita] = word
        else:
            raise Exception(f"El paciente tiene resultado de tratamiento inválido: {outcome}")
            
    alphabet_size = 2*(cant*2)
    if cod=='ternary':
        alphabet_size = 3*(cant*2)
    # Eliminar palabras inconsistentes
    D = {**deepcopy(Dp), **deepcopy(Dn)}
    for k in conflicts:
        if conflicts[k][0] > 0:
            for p_ID in D:
                for cita in D[p_ID]:
                    if D[p_ID][cita] == k:
                        if p_ID in Dp:
                            del Dp[p_ID][cita]
                        else:
                            del Dn[p_ID][cita]
    return Dp, Dn, alphabet_size
