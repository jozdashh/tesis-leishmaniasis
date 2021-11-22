#!/usr/bin/env python
# coding: utf-8

# ### Fecha
# 23 de Septiembre 2021
# 
# ### Objetivo
# Entrenar y evaluar ambos modelos, OIL y RPNI, con la versión más reciente de conjunto de células. A su vez, se busca probar las dos codificaciones establecidas (infra/sobre expresado e infra/sobre/estables) que hacen uso de la media y desviación estandar para clasificar cada gen, y diferentes tamaños para el alfabeto. Se busca entrenar 3 modelos diferentes para cada técnica con los 3 tipos de celulas correspondientes, y evaluar el desempeño de cada una de las combinaciones mencionadas en el parrafo anterior. Resultando todo esto en un total de 108 experimentos (54 para OIL y 54 para RPNI).
# 
# Finalmente se desarrolla un algoritmo integrador que usa los 3 tipos de modelos para cada paciente (si éste cuanta con la información de la célula correspondiente), y determina el diagnostico mediante un sistema de votación por mayoría. Solo se realiza una pequeña demostración de este algoritmo.
# 
# ### Resultado final
# Metricas de desempeño resultantes de todos los experimentos o todas combinaciones de variables ilustradas anteriormente.

# In[1]:


import RPNI as rpni
import OIL as oil
import utilities as ut
import seaborn as sns
import pandas as pd
import statistics as sts

from sklearn.model_selection import KFold
from numpy import array
from sklearn.metrics import confusion_matrix 
from sklearn.metrics import accuracy_score
from sklearn.metrics import precision_score
from sklearn.metrics import recall_score
from sklearn.metrics import f1_score

# Cargar datos y realizar primeros filtros/pre-procesamiento
# ==========================================================

# Resultados de tratamiento
patient_outcomes = pd.read_csv('data/20210118_EXP_ESPECIAL_TMRC3.csv', header=0, index_col=0)
patient_outcomes = patient_outcomes[['EF_LC_ESTADO_FINAL_ESTUDIO']]

# Neutrofilos
n_patients = pd.read_excel('data/neutrophil_cpm_before_batch-v202106.xlsx', header=0, index_col=0)
n_genes = pd.read_excel('data/neutrophil_clinical_table-v202106.xlsx', header=0, index_col=0)
n_genes = n_genes[['deseq_logfc', 'deseq_adjp']]
n_genes = n_genes[n_genes['deseq_adjp'] < 0.05].sort_values(by=['deseq_logfc'], ascending=False)

# Eosinofilos
e_patients = pd.read_excel('data/eosinophil_cpm_before_batch-v202106.xlsx', header=0, index_col=0)
e_genes = pd.read_excel('data/eosinophil_clinical_table-v202106.xlsx', header=0, index_col=0)
e_genes = e_genes[['deseq_logfc', 'deseq_adjp']]
e_genes = e_genes[e_genes['deseq_adjp'] < 0.05].sort_values(by=['deseq_logfc'], ascending=False)

# Monocitos
m_patients = pd.read_excel('data/monocyte_cpm_before_batch-v202106.xlsx', header=0, index_col=0)
m_genes = pd.read_excel('data/monocyte_clinical_table-v202106.xlsx', header=0, index_col=0)
m_genes = m_genes[['deseq_logfc', 'deseq_adjp']]
m_genes = m_genes[m_genes['deseq_adjp'] < 0.05].sort_values(by=['deseq_logfc'], ascending=False)
# Eliminar columna con defecto
m_patients.drop('X1034m2.', axis=1, inplace=True)


# In[2]:


# Visualizar resultados de tratamiento de pacientes

n_ID = list(n_patients.columns.values)
n_ID = [s.replace('n','d') for s in n_ID]
e_ID = list(e_patients.columns.values)
e_ID = [s.replace('e','d') for s in e_ID]
m_ID = list(m_patients.columns.values)
m_ID = [s.replace('m','d') for s in m_ID]
IDs = [s[:-2] for s in n_ID] + [s[:-2] for s in e_ID] + [s[:-2] for s in m_ID]
IDs = list(set(IDs))
IDs.sort()
l = list(map(lambda s:'SU'+s[1:],IDs))
l.remove('SU1017') # Este paciente no existe en la tabla de resultados de pacientes
patient_outcomes = patient_outcomes.loc[patient_outcomes.index.intersection(l)]
patient_outcomes


# ## Notas
# - 0: Curación definitiva, 1: Falla Terapeutica, 2: Perdida Durante el Seguimiento
# - El paciente 'SU1017' no se encuentra registrado en la tabla de resultados. Se omite para los experimentos.
# - Los pacientes 'SU1034' y 'SU2072' no tienen resultados conclusivos. Se omiten para los experimentos
# - El paciente 'SU1168' no tiene dato de final de estudio (NaN). Se omite.
# 
# - La cantidad de pacientes resultante es 10.

# In[3]:


omitted_patients = ['X1017','X1034','X2072','X1168']


# In[4]:


# Eliminar pacientes sin resultados concretos

n_cols = [c for c in n_patients.columns if c[:-2] not in omitted_patients]
e_cols = [c for c in e_patients.columns if c[:-2] not in omitted_patients]
m_cols = [c for c in m_patients.columns if c[:-2] not in omitted_patients]
n_patients = n_patients[n_cols]
e_patients = e_patients[e_cols]
m_patients = m_patients[m_cols]


# In[5]:


# Visualizar qué pacientes tienen los 3 tipos de células por cada cita

n_ID = list(n_patients.columns.values)
n_ID = [s.replace('n','d') for s in n_ID]
e_ID = list(e_patients.columns.values)
e_ID = [s.replace('e','d') for s in e_ID]
m_ID = list(m_patients.columns.values)
m_ID = [s.replace('m','d') for s in m_ID]
IDs = [s[:-2] for s in n_ID] + [s[:-2] for s in e_ID] + [s[:-2] for s in m_ID]
IDs = list(set(IDs))
IDs.sort()
data = list()
n_ID = set(n_ID)
e_ID = set(e_ID)
m_ID = set(m_ID)
for p_id in IDs:
    tmp = [0,0,0 , 0,0,0 , 0,0,0]
    for j in range(1,4): # por cada 3 citas
        if p_id+'d'+str(j) in n_ID: tmp[0+(j-1)] = 1
        if p_id+'d'+str(j) in e_ID: tmp[3+(j-1)] = 1
        if p_id+'d'+str(j) in m_ID: tmp[6+(j-1)] = 1
    data.append(tmp)
cm = sns.light_palette('green', as_cmap=True)
header = pd.MultiIndex.from_product([['Neutrófilos','Eosinófilos','Monocitos'],['C1','C2','C3']],names=['célula','cita'])
missing = pd.DataFrame(data, index=IDs, columns=header)
missing.style.background_gradient(cmap=cm, low=0, high=1, axis=None)


# ## RPNI y OIL
# Resultado de realizar las todas combinaciones de:
# - 3 tipos diferentes de célula
# - Algoritmos RPNI y OIL
# - Codificación de genes binaria y ternaria
# - Tamaños de alfabetos

# In[12]:


# Validación cruzada
# filename: nombre de archivo de palabras
# k: numero de bloques para validación cruzada
# algo: algoritmo para validación cruzada (oil/rpni)
def cross_validation(filename, k, algo):
    alphabet, X, y = ut.load_sample(filename)
    X = array(X)
    y = array(y)
    # Accuracy, precision, recall, f1 score
    results = {'acc':list(), 'pre':list(), 'rec':list(), 'f1s':list()}
    y_matrix = list()
    pred_matrix = list()
    kf = KFold(n_splits=k,shuffle=True,random_state=55)
    for train_index, test_index in kf.split(X,y):
        # Separar conjuntos de train y test
        X_train, X_test = X[train_index], X[test_index]
        y_train, y_test = y[train_index], y[test_index]
        filename_input = "samples/train_sample.in"
        filename_output = "automatas/train_automata.out"
        ut.write_sample(filename_input, len(alphabet), X_train, y_train)
        # Entrenamiento
        if algo == 'rpni':
            rpni.RPNI(filename_input, filename_output)
        elif algo == 'oil':
            oil.OIL(filename_input, filename_output)
        else:
            raise Exception(f"Algoritmo incorrecto: {algo}")
        # Prueba
        M, start_states, end_states = ut.load_automata(filename_output)
        pred = ut.predict(M, start_states, end_states, X_test)
        # Resultados
        results['acc'].append(accuracy_score(y_test,pred))
        results['pre'].append(precision_score(y_test,pred))
        results['rec'].append(recall_score(y_test,pred))
        results['f1s'].append(f1_score(y_test,pred))
        for out in y_test:
            y_matrix.append(out)
        for out in pred:
            pred_matrix.append(out)
    print(f"Confusion matrix")
    print(confusion_matrix(y_matrix, pred_matrix))
    print()
    return results


# In[13]:


# Cantidades de genes para pre-procesamiento
gene_amount = [6,8,10,12,14,16,20,30,40]


# In[14]:


# Diccionario de dataframes para resumir resultados de experimentos (celula x modelo x cod x sizes)
df_results = dict()
header = pd.MultiIndex.from_product([['accuracy','precision','recall','f1'], ['mean', 'std']])


# # La cantidad de bloques usada para toda la validación cruzada es 4

# In[15]:


print("N: neutrófilos, E: eosinófilos, M: monocitos\n")
for cell, x_patients, x_genes in [('n',n_patients,n_genes),('e',e_patients,e_genes),('m',m_patients,m_genes)]:
    print(f"\nCÉLULA: {cell.upper()}\n")
    for algo in ['rpni','oil']:
        for cod in ['binary','ternary']:
            print(f"{algo.upper()} - {cod.upper()}\n")
            df_data = [ list() for _ in gene_amount ]
            i = 0
            for size in gene_amount:
                if cod=='binary':
                    print(f"{2*size} símbolos")
                else:
                    print(f"{3*size} símbolos")
                Dp, Dn, alphabet_size = ut.pre_process(x_patients,x_genes,patient_outcomes,size//2,cod)
                X, y = ut.flatten(Dp,Dn)
                print(f"0:{y.count(0)}, 1:{y.count(1)}")
                filename = "samples/complete-sample.in"
                ut.write_sample(filename,alphabet_size,X,y)
                results = cross_validation(filename,4,algo)
                df_data[i].append(round(sts.mean(results['acc']),3))
                df_data[i].append(round(sts.stdev(results['acc']),3))
                df_data[i].append(round(sts.mean(results['pre']),3))
                df_data[i].append(round(sts.stdev(results['pre']),3))
                df_data[i].append(round(sts.mean(results['rec']),3))
                df_data[i].append(round(sts.stdev(results['rec']),3))
                df_data[i].append(round(sts.mean(results['f1s']),3))
                df_data[i].append(round(sts.stdev(results['f1s']),3))
                i += 1
            index_ = list()
            if cod == 'binary':
                index_ = [2*j for j in gene_amount]
            else:
                index_ = [3*j for j in gene_amount]
            df_results[(cell,algo,cod)] = pd.DataFrame(df_data, index=index_, columns=header)


# In[16]:


df_results[('n','rpni','binary')]


# In[17]:


df_results[('n','rpni','ternary')]


# In[18]:


df_results[('e','rpni','binary')]


# In[19]:


df_results[('e','rpni','ternary')]


# In[20]:


df_results[('m','rpni', 'binary')]


# In[21]:


df_results[('m','rpni','ternary')]


# In[22]:


df_results[('n','oil','binary')]


# In[23]:


df_results[('n','oil','ternary')]


# In[24]:


df_results[('e','oil','binary')]


# In[25]:


df_results[('e','oil','ternary')]


# In[26]:


df_results[('m','oil','binary')]


# In[27]:


df_results[('m','oil','ternary')]


# # Algoritmo Integrador
# Tomar decision final segun votacion de modelos disponibles y desempeño previo de los mismos

# In[28]:


# Obtener modelos con mejores resultados
best_models = dict()


# In[29]:


for cell in ['n','e','m']:
    models = list()
    for algo in ['rpni', 'oil']:
        res1 = df_results[(cell,algo,'binary')]['f1']['mean']
        res2 = df_results[(cell,algo,'ternary')]['f1']['mean']
        models.append( (res1.idxmax()//2,'binary',algo,res1.loc[res1.idxmax()]) )
        models.append( (res2.idxmax()//3,'ternary',algo,res2.loc[res2.idxmax()]) )
    best_models[cell] = max(models,key=lambda x: x[3]) # Seleccionar el que tenga mayor f1 score


# In[30]:


# célula: (cant_de_genes, codificación, algoritmo, f1 score)
best_models


# # Se excluyen los pacientes SU2073 (falla) y SU2065 (cura) para hacer una pequeña demostración del algoritmo

# In[31]:


excluded_patients = ['X2052', 'X2071']
#excluded_patients = []
new_n_D = dict()
new_e_D = dict()
new_m_D = dict()


# In[32]:


Dp, Dn, n_alphabet_size = ut.pre_process(n_patients,n_genes,patient_outcomes,best_models['n'][0]//2,best_models['n'][1])
for p in excluded_patients:
    if p in Dp:
        new_n_D[p] = Dp[p]
        del Dp[p]
    elif p in Dn:
        new_n_D[p] = Dn[p]
        del Dn[p]
X, y = ut.flatten(Dp,Dn)
for x1,y1 in zip(X,y):
    print(x1,y1)
filename_input = "samples/complete-data-n.in"
filename_output = "automatas/integration_automata_n.out"
ut.write_sample(filename_input,n_alphabet_size,X,y)
if best_models['n'][2]=='rpni':
    rpni.RPNI(filename_input,filename_output)
else:
    oil.OIL(filename_input,filename_output)
n_M, n_ss, n_es = ut.load_automata(filename_output)
n_model = {'automata':n_M, 'start_states':n_ss, "end_states":n_es}


# In[33]:


Dp, Dn, e_alphabet_size = ut.pre_process(e_patients,e_genes,patient_outcomes,best_models['e'][0]//2,best_models['e'][1])
for p in excluded_patients:
    if p in Dp:
        new_e_D[p] = Dp[p]
        del Dp[p]
    elif p in Dn:
        new_e_D[p] = Dn[p]
        del Dn[p]
X, y = ut.flatten(Dp,Dn)
for x1,y1 in zip(X,y):
    print(x1,y1)
filename_input = "samples/complete-data-e.in"
filename_output = "automatas/integration_automata_e.out"
ut.write_sample(filename_input,e_alphabet_size,X,y)
if best_models['n'][2]=='rpni':
    rpni.RPNI(filename_input,filename_output)
else:
    oil.OIL(filename_input,filename_output)
e_M, e_ss, e_es = ut.load_automata(filename_output)
e_model = {'automata':e_M, 'start_states':e_ss, "end_states":e_es}


# In[34]:


Dp, Dn, m_alphabet_size = ut.pre_process(m_patients,m_genes,patient_outcomes,best_models['m'][0]//2,best_models['m'][1])
for p in excluded_patients:
    if p in Dp:
        new_m_D[p] = Dp[p]
        del Dp[p]
    elif p in Dn:
        new_m_D[p] = Dn[p]
        del Dn[p]
X, y = ut.flatten(Dp, Dn)
for x1,y1 in zip(X,y):
    print(x1,y1)
filename_input = "samples/complete-data-m.in"
filename_output = "automatas/integration_automata_m.out"
ut.write_sample(filename_input,m_alphabet_size,X,y)
if best_models['n'][2]=='rpni':
    rpni.RPNI(filename_input,filename_output)
else:
    oil.OIL(filename_input,filename_output)
m_M, m_ss, m_es = ut.load_automata(filename_output)
m_model = {'automata':m_M, 'start_states':m_ss, "end_states":m_es}


# In[35]:


# f1: nombre de archivo que contiene palabra de neutrofilos
# f2: nombre de archivo que contiene palabra de eosinófilos
# f3: nombre de archivo que contiene palabra de monocitos
# n_model: automata y estados iniciales y finales de nuetrófilos
# e_model: automata y estados iniciales y finales de eosinófilos
# m_model: automata y estados iniciales y finales de monocitos
# f1_score: diccionario que indica el score f1 de cada modelo según validación cruzada

def integration_algorithm(f1, f2, f3, n_model, e_model, m_model, f1_score):
    vote = dict()
    if f1 != None:
        alphabet_size_1, X1, y1 = ut.load_sample(f1)
        print(f"word n: {X1[0]}")
        vote['n'] = ut.predict(n_model['automata'],n_model['start_states'],n_model['end_states'], X1)[0]
    if f2 != None:
        alphabet_size_2, X2, y2 = ut.load_sample(f2)
        print(f"word e: {X2[0]}")
        vote['e'] = ut.predict(e_model['automata'],e_model['start_states'],e_model['end_states'], X2)[0]
    if f3 != None:
        alphabet_size_3, X3, y3 = ut.load_sample(f3)
        print(f"word m: {X3[0]}")
        vote['m'] = ut.predict(m_model['automata'],m_model['start_states'],m_model['end_states'], X3)[0]
    print('votes:')
    print(vote)
    results = list(vote.values())
    failure = results.count(1)
    success = results.count(0)
    if failure > success:
        veredict = 1
    elif failure < success:
        veredict = 0
    # Si la cantidad de votos positivos y negativos es igual, quiere decir que el paciente solo tiene
    # la palabra de 2 células, y la predicción de cada modelo respectivo es desigual.
    # En otras palabras, hay un empate.
    elif failure == success:
        model_a, model_b = vote.keys()
        if f1_score[model_a] > f1_score[model_b]:
            veredict = vote[model_a]
        else:
            veredict = vote[model_b]
    print(f"veredict: {veredict}")
    return veredict


# In[36]:


# Pequeña demostración de algoritmo con 2 pacientes

f1_score = {'n':best_models['n'][3], 'e':best_models['e'][3], 'm':best_models['m'][3]}
for p in excluded_patients:
    print(f"\nPaciente: {p}\n")
    for c in ['C1', 'C2', 'C3']:
        f1,f2,f3 = None,None,None
        if p in new_n_D and c in new_n_D[p]:
            f1 = 'samples/patient_n.in'
            X = [ new_n_D[p][c] ]
            y = [0] # El paciente enrealidad no tiene valor de salida. Se pone 0 por default.
            ut.write_sample(f1,n_alphabet_size,X,y)
        if p in new_e_D  and c in new_e_D[p]:
            f2 = 'samples/patient_e.in'
            X = [ new_e_D[p][c] ]
            y = [0]
            ut.write_sample(f2,e_alphabet_size,X,y)
        if p in new_m_D  and c in new_m_D[p]:
            f3 = 'samples/patient_m.in'
            X = [ new_m_D[p][c] ]
            y = [0]
            ut.write_sample(f3,m_alphabet_size,X,y)
        if (f1!=None) or (f2!=None) or (f3!=None):
            print(f"Cita #{c[1]}")
            veredict = integration_algorithm(f1, f2, f3, n_model, e_model, m_model, f1_score)
        else:
            print(f"No hay datos de cita #{c[1]}")
        print()


# In[ ]:




