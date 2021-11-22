#!/usr/bin/env python
# coding: utf-8

# In[1]:


import utilities as ut
import pandas as pd
from numpy import array


# In[2]:


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


# In[7]:


# Cantidades de genes para pre-procesamiento
gene_amount = [6,8,10,12,14,16,20,30,40]


# In[8]:


print("N: neutrófilos, E: eosinófilos, M: monocitos\n")
for cell, x_patients, x_genes in [('n',n_patients,n_genes),('e',e_patients,e_genes),('m',m_patients,m_genes)]:
    print(f"\nCÉLULA: {cell.upper()}\n")
    for size in gene_amount:
        ut.pre_process(x_patients,x_genes,patient_outcomes,size//2,"binary")

