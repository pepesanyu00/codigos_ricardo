#!/usr/bin/python3
# -*- coding: utf-8 -*-
import sys
import os #Para quitar directorios y extensiones de un path
import glob #Para obtner una lista de archivos de un path con wildcards
import numpy as np #Package for scientific computing with Python
import matplotlib.pyplot as plt #Librería gráfica
from subprocess import call #Para llamar a un comando del shell

sys.path.append('/home/quislant/Ricardo/Research/myPyLib')
import parseStatsHaswell as psf

titlesDict = {"audio-MPIII-SVD": "Audio",
              "power-MPIII-SVF_n180000": "Power",
              "seismology-MPIII-SVE_n180000": "Seismology",
              "e0103_n180000": "ECG",
              "penguin_sample_TutorialMPweb": "Penguin",
              "human_activity-MPIII-SVC": "Human activity"}

# Esta función lee los archivos en la lista de archivos que se genera a partir
# de filePath (que tendrá wildcards) y devuelve la media del tiempo de ejecución
def readTimeAvg(filePath):
	fileList = glob.glob(filePath)
	timeAcc = 0.0
	n = len(fileList)
	if n == 0:
		print("No hay archivos %s" % filePath)
		return timeAcc
	for file in fileList:
		f = open(file, 'r')
		tmp = f.readline() # Leo el primer comentario
		timeAcc = timeAcc + float(f.readline()) # Leo el tiempo y lo paso a float
		f.close()
	return timeAcc/len(fileList)

#Parseo los argumentos
if len(sys.argv) == 4:
	tseries = sys.argv[1]
	# Le quito el directorio y el .txt si los tiene
	tseries = os.path.basename(tseries)
	tseries = os.path.splitext(tseries)[0]
	w = int(sys.argv[2])
	legend = int(sys.argv[3])
	print("Plotting results for tseries %s with window size %d and %d legend ..." % (tseries, w, legend))
else:
	print("Uso: ./plotSpeedup.py timeseries windowSize legend")
	exit(-1)

direc="../results/"
# [legend label, file]
linesv = [["TM"  , "scampTM_%s_w%d_t%d_x%d_*", "scampTM_%s.txt_%d_%d_%d_*"],]
numThreads = (1,)
xactSize = (1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048)
x = range(1,len(xactSize)+1)

lfs = 20 #33 #label font size
ms = 2   #marker size
markers = ('o', '^', 'v', 's', '*', 'p', 'h', '<', '>', '8', 'H', 'D', 'd', None)
mksize = (ms*6,ms*7,ms*7,ms*6,ms*8,ms*7,ms*6,ms*7,ms*6)
#colors = plt.cm.Set1(range(ini,fin,int((fin-ini)/len(linesv)) ))
set1 = plt.cm.get_cmap('Set1') #Obtengo el color map
colors = set1(np.linspace(0.0,1,len(linesv)+7)) #Eligo los colores (cambiar ini y fin para variar los colores)
#En blanco y negro (hago la media)
#colors = [[sum(x)/3.]*3 for x in colors]

#Extraigo el tiempo del secuencial (se tomará para el secuencial el scamp no-vect con 1 hilo)
tSeq = readTimeAvg(direc + ("scamp_%s_w%d_t%d_*"%(tseries,w,1)))
for j in range(len(linesv)):
  timePerTh = []
  commitsTh = []
  abortsTh = []
  fallbacksTh = []
  capAbortsTh = []
  retriesTh = []
  # Obtengo el tiempo para cada número de hilos
  for d in numThreads:
    for xs in xactSize:
      timePerTh.append(readTimeAvg(direc + (linesv[j][1]%(tseries,w,d,xs))))
      hwstats = psf.StatsAvg(direc, linesv[j][2]%(tseries,w,d,xs), False)
      #hwstats.ParseCommitsAvg()
      #hwstats.ParseAbortsAvg()
      hwstats.ParseAbortsAvg()
      hwstats.ParseCommitsAvg()
      hwstats.ParseFallbacksAvg()
      abortsTh.append(hwstats.GetAbortsAvg())
      commitsTh.append(hwstats.GetCommitsAvg())
      fallbacksTh.append(hwstats.GetFallbacksAvg())
      #retriesTh.append(hwstats.GetRetriesAvg())
      capAbortsTh.append(hwstats.GetCapacityAbortsAvg())

  #Convierto las listas en arrays para hacer cálculos más fácilmente (np.array)
  timePerTh = np.array(timePerTh)
  commitsTh = np.array(commitsTh)
  fallbacksTh = np.array(fallbacksTh)
  abortsTh = np.array(abortsTh)
  #retriesTh = np.array(retriesTh)
  capAbortsTh = np.array(capAbortsTh)
  print(abortsTh)
  print(capAbortsTh)
  plt.bar(x, (abortsTh+commitsTh)/(abortsTh[0]+commitsTh[0]),0.8,
           color=colors[j], label="#tx")
  plt.plot(x, tSeq/timePerTh, color=colors[j+1], label="Speedup", linewidth=ms,
            marker=markers[j], markeredgecolor=colors[j+1], markersize=mksize[j],
            markeredgewidth=1)
  plt.plot(x, capAbortsTh/(abortsTh+commitsTh), color=colors[j+2], label="CAR",
            linewidth=ms, marker=markers[j+1], markeredgecolor=colors[j+2],
            markersize=mksize[j+1], markeredgewidth=1)
  #print(fallbacksTh+commitsTh)
  plt.grid(axis='y', linewidth=1, linestyle="-")
  #plt.ylabel('Speedup', fontsize=lfs)
  plt.xlabel('Xact Size', fontsize=lfs)
  plt.title(titlesDict[tseries] + ' w=' + str(w), fontsize=lfs)
  plt.xticks(x,xactSize,fontsize=lfs*0.8,rotation=90)
  plt.yticks(fontsize=lfs*0.8)
  if (legend != 0):
    plt.legend(frameon=False, fontsize=lfs*0.9, ncol=1, columnspacing=1, loc="upper center")

ax = plt.gca()
ax.set_aspect(9)
plt.savefig("./scamp_xactSens_%s-w%d.pdf" % (tseries,w), format='pdf',bbox_inches='tight')
plt.savefig("./scamp_xactSens_%s-w%d.png" % (tseries,w), format='png',bbox_inches='tight')
#call("cp ../z_figs/SU.pdf ../../paper-TPDS/figs/", shell=True)
#plt.show()

