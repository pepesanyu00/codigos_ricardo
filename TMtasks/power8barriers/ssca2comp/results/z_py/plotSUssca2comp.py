#!/usr/bin/python
# -*- coding: utf-8 -*-
import sys
# Importo la clase que lee los ficheros de estadísticas de gems
sys.path.append('/home/quislant/Ricardo/Research/myPyLib/')
import numpy as np #Fundamental package for scientific computing with Python
import matplotlib.pyplot as plt #Librería gráfica
#Importo la clase StatsFile de mi librería de parsing
import parseStatsHaswell as psf
from subprocess import call #Para llamar a un comando del shell

direc="../"
params=["s13-i1.0-u1.0-l3-p3", "s14-i1.0-u1.0-l9-p9", "s20-i1.0-u1.0-l3-p3",]
titles = ["\\textbf{SSCA2 k1 -s13-i1.0-u1.0-l3-p3}",
          "\\textbf{SSCA2 k1 -s14-i1.0-u1.0-l9-p9}",
          "\\textbf{SSCA2 k1 -s20-i1.0-u1.0-l3-p3}",]
# [synchronization method, label, file]
files = [["TM", "TM", "ssca2comp_%s-%s-t%d-*"],
         ["SB", "SB", "ssca2comp_%s-%s-t%d-*"],]

numThreads = (1, 2, 4, 8, 16, 32, 64, 128)
N = len(numThreads)
ind = np.arange(N)  # the x locations for the groups
width = 0.111       # the width of the bars, para 7 barras, dejo la primera en blanco

#Lista de texturas (hatch - sombreado) repitiendo el carácter se consigue más densidad
patterns = ('-', '++', 'x', '\\\\', '//', '*', 'o', 'O', '.', None)
markers = ('o', '^', 'v', 's', 'p', '*', 'h', '<', '>', '8', 'H', 'D', 'd', None)
lines = ('--', '-.', ':', '-')
dash = ((15, 15), (10,5,2,5), (5,5)) #points of dash, blank, dash, blank
dgc=(0.3, 0.3, 0.3) #dark gray color
lgc=(0.8, 0.8, 0.8) #light gray
blw = 3 #bar linewidth
lfs = 30#33 #label font size
plt.rc('text', usetex=True) #Le digo que el procesador de textos sea latex (se pueden usar fórmulas)
plt.rc('font', family='sans-serif') #Le digo que la fuente por defecto sea sans-serif
plt.rc('figure', autolayout=True) #Para que no se corten las etiquetas de los ejes (calcula el bouding box automáticamente)
ini=140
fin=10
colors = plt.cm.Set1(range(ini,fin,-(ini-fin)/(len(files)+3))) #Pongo +3 para coincidir con recurrence
mksize = (blw*5,blw*6,blw*6,blw*5,blw*6)
#En blanco y negro (hago la media)
#colors = [[sum(x)/3.]*3 for x in colors]
for i in range(len(params)):
  tmp = psf.StatsAvg(direc, (files[0][2]%("SEQ",params[i],1)), True)
  seq = np.array(tmp.GetTimeAvg())
  ax = plt.subplot(1, 3, i+1) #nrows, ncols, plot_number
  for j in range(len(files)):
    fbackPriv = []
    for d in numThreads:
      tmp = psf.StatsAvg(direc, (files[j][2]%(files[j][0],params[i],d)), False)
      tmp.ParseTimeAvg()
      fbackPriv.append(tmp.GetTimeAvg())
    #Convierto las listas en arrays para hacer cálculos más fácilmente (np.array)
    fbackPriv = np.array(fbackPriv)
    ax.plot(ind, seq/fbackPriv, color=colors[j+2], label=files[j][1], linewidth=blw,
            marker=markers[j+2], markeredgecolor=[0,0,0], markersize=mksize[j+2],
            markeredgewidth=blw)
  
  ax.spines["right"].set_visible(False)
  ax.spines["top"].set_visible(False)
  #ax.spines["left"].set_linewidth(1)
  #ax.spines["bottom"].set_linewidth(1)
  ax.get_xaxis().tick_bottom()
  ax.get_yaxis().tick_left()
  ax.yaxis.grid(color=lgc, linewidth=1, linestyle="-")
  ax.set_axisbelow(True) #Para que el grid no se muestre por encima de las barras
  #print ax.yaxis.label.get_fontname()
  
  # add some text for labels, title and axes ticks if i == 0:
  ax.set_ylabel('Speedup', fontsize=lfs)
  ax.set_xlabel(r'\#TH', fontsize=lfs)
  ax.set_title(titles[i], fontsize=lfs)
  ax.set_xlim([-0.3, len(numThreads)-0.7])
  ax.set_xticks(ind)
  ax.set_xticklabels(('1', '2', '4', '8', '16', '32', '64', '128'), fontsize=lfs*0.8)
  ymax = np.ceil(max(ax.get_yticks()))
  ymax = ymax + (ymax == 1) # Si ymax es uno le sumo uno
  ax.set_yticks(range(0,int(ymax)+1,1))
  ax.set_yticklabels(range(0,int(ymax)+1,1), fontsize=lfs*0.8)
  ax.set_aspect(5/ymax)
#  if i == 0: #Si es el último imprimo la leyenda
#     lgd=ax.legend(frameon=False, fontsize=lfs, ncol=2, bbox_to_anchor=(2.1, -0.15), columnspacing=1)


#plt.tight_layout()#w_pad=10)#h_pad=-30
#plt.subplots_adjust(hspace=-0.9)
#Pongo la figura más grande, ya que los patrones no se ven afectados por el tamaño
#de las gráficas, y cuantas más se meten en una figura más pequeñas son las gráficas
#(no se cambia el tamaño del patrón, no sé pq)
fig = plt.gcf()
si = fig.get_size_inches()
fig.set_size_inches(si[0]*3, si[1])

plt.savefig("../z_figs/SUssca2compP8.pdf", format='pdf', bbox_inches='tight')
plt.savefig("../z_figs/SUssca2compP8.png", format='png', bbox_inches='tight')
#call("cp ../z_figs/SU.pdf ../../paper-TPDS/figs/", shell=True)
#plt.show()

