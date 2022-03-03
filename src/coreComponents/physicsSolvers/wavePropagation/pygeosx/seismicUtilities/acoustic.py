import pygeosx
import numpy as np
import os
import h5py
import segyio


def recomputeSourceAndReceivers(solver, sources, receivers):
    updateSourceAndReceivers(solver, sources, receivers)

    solver.reinit()


def updateSourceAndReceivers( solver, sources_list=[], receivers_list=[] ):
    src_pos_geosx = solver.get_wrapper("sourceCoordinates").value()
    src_pos_geosx.set_access_level(pygeosx.pylvarray.RESIZEABLE)

    rcv_pos_geosx = solver.get_wrapper("receiverCoordinates").value()
    rcv_pos_geosx.set_access_level(pygeosx.pylvarray.RESIZEABLE)


    src_pos_geosx.resize(len(sources_list))
    if len(sources_list) == 0:
        src_pos_geosx.to_numpy()[:] = np.zeros((0,3))
    else:
        src_pos = [source.coords for source in sources_list]
        src_pos_geosx.to_numpy()[:] = src_pos[:]

    rcv_pos_geosx.resize(len(receivers_list))
    if len(receivers_list) == 0:
        rcv_pos_geosx.to_numpy()[:] = np.zeros((0,3))
    else:
        rcv_pos = [receiver.coords for receiver in receivers_list]
        rcv_pos_geosx.to_numpy()[:] = rcv_pos[:]

    solver.reinit()


def updateSourceValue( solver, value ):
    src_value = solver.get_wrapper("sourceValue").value()
    src_value.set_access_level(pygeosx.pylvarray.MODIFIABLE)
    src_value.to_numpy()[:] = value[:]



def resetWaveField(group):
    group.get_wrapper("Solvers/acousticSolver/indexSeismoTrace").value()[0] = 0
    nodeManagerPath = "domain/MeshBodies/mesh/meshLevels/Level0/nodeManager/"

    pressure_nm1 = group.get_wrapper(nodeManagerPath + "pressure_nm1").value()
    pressure_nm1.set_access_level(pygeosx.pylvarray.MODIFIABLE)

    pressure_n = group.get_wrapper(nodeManagerPath + "pressure_n").value()
    pressure_n.set_access_level(pygeosx.pylvarray.MODIFIABLE)

    pressure_np1 = group.get_wrapper(nodeManagerPath + "pressure_np1").value()
    pressure_np1.set_access_level(pygeosx.pylvarray.MODIFIABLE)

    pressure_geosx = group.get_wrapper("Solvers/acousticSolver/pressureNp1AtReceivers").value()
    pressure_geosx.set_access_level(pygeosx.pylvarray.MODIFIABLE)

    pressure_nm1.to_numpy()[:] = 0.0
    pressure_n.to_numpy()[:]   = 0.0
    pressure_np1.to_numpy()[:] = 0.0
    pressure_geosx.to_numpy()[:] = 0.0



def setTimeVariables(problem, maxTime, dt, dtSeismoTrace):
    problem.get_wrapper("Events/maxTime").value()[0] = maxTime
    problem.get_wrapper("Events/solverApplications/forceDt").value()[0] = dt
    problem.get_wrapper("/Solvers/acousticSolver/dtSeismoTrace").value()[0] = dtSeismoTrace



def forward(problem, solver, collections, outputs, shot, maxTime, outputWaveFieldInterval, rank=0):
    gradDir = "partialGradient"
    costDir = "partialCostFunction"

    if rank == 0:
        if os.path.exists(gradDir):
            pass
        else:
            os.mkdir(gradDir)

    #To be changed once GEOSX can output multiple variables in same HDF5 file
    outputs[0].setOutputName(os.path.join(gradDir,"forwardWaveFieldNp1_"+shot.id))
    outputs[1].setOutputName(os.path.join(gradDir,"forwardWaveFieldN_"+shot.id))
    outputs[2].setOutputName(os.path.join(gradDir,"forwardWaveFieldNm1_"+shot.id))

    #Get view on pressure at receivers locations
    pressureAtReceivers = solver.get_wrapper("pressureNp1AtReceivers").value()

    #update source and receivers positions
    updateSourceAndReceivers(solver, shot.sources.source_list, shot.receivers.receivers_list)
    pygeosx.apply_initial_conditions()

    shot.flag = "In Progress"

    time = 0.0
    dt = shot.dt
    i = 0

    if rank == 0 :
        print("\nForward")

    #Time loop
    while time < maxTime:
        if rank == 0:
            print("time = %.3fs," % time, "dt = %.4f," % dt, "iter =", i+1)

        #Execute one time step
        solver.execute(time, dt)

        time += dt
        i += 1

        #Collect/export waveField values to hdf5
        if i % outputWaveFieldInterval == 0:
            collections[0].collect(time, dt)
            outputs[0].output(time, dt)

            collections[1].collect(time, dt)
            outputs[1].output(time, dt)

            collections[2].collect(time, dt)
            outputs[2].output(time, dt)

    pressure = np.array(pressureAtReceivers.to_numpy())

    #Residual computation
    residualTemp = computeResidual("dataTest/seismo_Shot"+shot.id+".sgy", pressure)
    residual = residualLinearInterpolation(residualTemp, maxTime, dt, dtSeismoTrace)

    if rank == 0:
        computePartialCostFunction(costDir, residualTemp, shot)

    #Reset waveField
    resetWaveField(problem)



def computeResidual(filename, data_obs):
    residual = np.zeros(data_obs.shape)
    with segyio.open(filename, 'r+', ignore_geometry=True) as f:
        for i in range(data_obs.shape[1]):
            residual[:, i] = f.trace[i] - data_obs[:, i]
        f.close()

    return residual


def residualLinearInterpolation(rtemp, maxTime, dt, dtSeismoTrace):
    r = np.zeros((int(maxTime/dt)+1, np.size(rtemp,1)))
    for i in range(np.size(rtemp,1)):
        r[:,i] = np.interp(np.linspace(0, maxTime, int(maxTime/dt)+1), np.linspace(0, maxTime, int(maxTime/dtSeismoTrace)+1), rtemp[:,i])

    return r


def computePartialCostFunction(directory_in_str, residual, shot):
    if os.path.exists(directory_in_str):
        pass
    else:
        os.mkdir(directory_in_str)

    partialCost = 0
    for i in range(residual.shape[1]):
        partialCost += np.linalg.norm(residual[:, i])

    partialCost = partialCost/2

    with open(os.path.join(directory_in_str, "partialCost_"+shot.id+".txt"), 'w') as f:
        f.write(str(partialCost))

    f.close()


def computeFullCostFunction(directory_in_str, acquisition):
    directory = os.fsencode(directory_in_str)
    nfiles = len(acquisition.shots)

    n=0
    fullCost=0
    while n < nfiles:
        file_list = os.listdir(directory)
        if len(file_list) != 0:
            filename = os.fsdecode(file_list[0])
            costFile = open(os.path.join(directory_in_str,filename), "r")
            fullCost += float(costFile.readline())
            costFile.close()
            os.remove(os.path.join(directory_in_str,filename))
            n += 1
        else:
            continue

    os.rmdir(directory_in_str)

    return fullCost


def computePartialGradient(directory_in_str, shot):
    if os.path.exists(directory_in_str):
        pass
    else:
        os.mkdir(directory_in_str)

    h5fNp1 = h5py.File(os.path.join(directory_in_str,"forwardWaveFieldNp1_"+shot.id+".hdf5"), "r+")
    h5bNp1 = h5py.File(os.path.join(directory_in_str,"backwardWaveFieldNp1_"+shot.id+".hdf5"), "r+")

    h5fN = h5py.File(os.path.join(directory_in_str,"forwardWaveFieldN_"+shot.id+".hdf5"), "r+")
    h5bN = h5py.File(os.path.join(directory_in_str,"backwardWaveFieldN_"+shot.id+".hdf5"), "r+")

    h5fNm1 = h5py.File(os.path.join(directory_in_str,"forwardWaveFieldNm1_"+shot.id+".hdf5"), "r+")
    h5bNm1 = h5py.File(os.path.join(directory_in_str,"backwardWaveFieldNm1_"+shot.id+".hdf5"), "r+")

    keysNp1 = list(h5fNp1.keys())
    keysN = list(h5fN.keys())
    keysNm1 = list(h5fNm1.keys())

    for i in range(len(keysNp1)-2):
        if ("ReferencePosition" in keysNp1[i]) or ("Time" in keysNp1[i]):
            del h5fNp1[keysNp1[i]]
            del h5bNp1[keysNp1[i]]

            del h5fN[keysNp1[i]]
            del h5bN[keysNp1[i]]

            del h5fNm1[keysNp1[i]]
            del h5bNm1[keysNp1[i]]

    keysNp1 = list(h5fNp1.keys())

    h5p = h5py.File(os.path.join(directory_in_str,"partialGradient_"+shot.id+".hdf5"), "w")

    h5p.create_dataset("ReferencePosition", data = h5fNp1[keysNp1[-2]][0], dtype='d')
    h5p.create_dataset("Time", data = h5fNp1[keysNp1[-1]], dtype='d')
    h5p.create_dataset("partialGradient", data = np.zeros(h5bNp1[keysNp1[0]][0].shape[0]), dtype='d')


    keyp = list(h5p.keys())

    n = len( list( h5fNp1[keysNp1[0]] ) )
    for i in range(n):
        h5p["partialGradient"][:] -= (h5fNp1["pressure_np1"][i, :] - 2*h5fN["pressure_n"][i, :] + h5fNm1["pressure_nm1"][i, :]) * h5bN["pressure_n"][-i-1, :] / (shot.dt*shot.dt)


    h5fNp1.close()
    h5bNp1.close()
    h5fN.close()
    h5bN.close()
    h5fNm1.close()
    h5bNm1.close()
    h5p.close()

    os.remove(os.path.join(directory_in_str,"forwardWaveFieldNp1_"+shot.id+".hdf5"))
    os.remove(os.path.join(directory_in_str,"backwardWaveFieldNp1_"+shot.id+".hdf5"))
    os.remove(os.path.join(directory_in_str,"forwardWaveFieldN_"+shot.id+".hdf5"))
    os.remove(os.path.join(directory_in_str,"backwardWaveFieldN_"+shot.id+".hdf5"))
    os.remove(os.path.join(directory_in_str,"forwardWaveFieldNm1_"+shot.id+".hdf5"))
    os.remove(os.path.join(directory_in_str,"backwardWaveFieldNm1_"+shot.id+".hdf5"))


def computeFullGradient(directory_in_str, acquisition):
    directory = os.fsencode(directory_in_str)

    limited_aperture_flag = acquisition.limited_aperture
    nfiles = len(acquisition.shots)

    n=0
    while True:
        file_list = os.listdir(directory)
        if len(file_list) != 0:
            filename = os.fsdecode(file_list[0])
            h5p = h5py.File(os.path.join(directory_in_str,filename), "r")
            keys = list(h5p.keys())
            n += 1
            break
        else:
            continue

    h5F = h5py.File("fullGradient.hdf5", "w")
    h5F.create_dataset("fullGradient", data = h5p["partialGradient"], dtype='d', chunks=True, maxshape=(None,))
    h5F.create_dataset("ReferencePosition", data = h5p["ReferencePosition"], chunks=True, maxshape=(None, 3))
    h5F.create_dataset("Time", data = h5p["Time"])
    keysF = list(h5F.keys())

    h5p.close()
    os.remove(os.path.join(directory_in_str,filename))

    while True:
        file_list = os.listdir(directory)
        if n == nfiles:
            break
        elif len(file_list) > 0:
            for file in os.listdir(directory):
                filename = os.fsdecode(file)
                if filename.startswith("partialGradient"):
                    h5p = h5py.File(os.path.join(directory_in_str, filename), 'r')
                    keysp = list(h5p.keys())
                    print(h5p["ReferencePosition"][:], "\n")

                    if limited_aperture_flag:
                        indp = 0
                        ind = []
                        for coordsp in h5p["ReferencePosition"]:
                            indF = 0
                            flag = 0
                            for coordsF in h5F["ReferencePosition"]:
                                if not any(coordsF - coordsp):
                                    h5F["fullGradient"][indF] += h5p["partialGradient"][indp]
                                    flag = 1
                                    break
                                indF += 1
                            if flag == 0:
                                ind.append(indp)
                            indp += 1

                        if len(ind) > 0:
                            h5F["fullGradient"].resize(h5F["fullGradient"].shape[0] + len(ind), axis=0)
                            h5F["fullGradient"][-len(ind):] = h5p["partialGradient"][ind]

                            h5F["ReferencePosition"].resize(h5F["ReferencePosition"].shape[0] + len(ind), axis=0)
                            h5F["ReferencePosition"][-len(ind):] = h5p["ReferencePosition"][ind]

                    else:
                        h5F["fullGradient"][:] += h5p["partialGradient"][:]

                    h5p.close()
                    os.remove(os.path.join(directory_in_str,filename))
                    n+=1
                else:
                    continue

        else:
            continue

    ind = np.lexsort((h5F["ReferencePosition"][:, 2], h5F["ReferencePosition"][:, 1], h5F["ReferencePosition"][:, 0]))

    tempPos = np.zeros( h5F["ReferencePosition"].shape)
    tempVal = np.zeros( h5F["fullGradient"].shape)
    for i in range(len(ind)):
        tempPos[i] = h5F["ReferencePosition"][ind[i]]
        tempVal[i] = h5F["fullGradient"][ind[i]]

    h5F["ReferencePosition"][:] = tempPos[:]
    h5F["fullGradient"][:] = tempVal[:]

    h5F.close()
    os.rmdir(directory_in_str)

    return h5F



def armijoCondition(J, Jm, d, gradJ, alpha, k1=0.8):
    success = (Jm <= k1 * alpha * np.dot(d, gradJ))
    return success

def goldsteinCondition(J, Jm, d, gradJ, alpha, k2=0.2):
    success = (Jm >= k2 * alpha * np.dot(d, gradJ))
    return success


def descentDirection(gradJ, method="steepestDescent"):
    if method == "steepestDescent":
        d = -gradJ
    else:
        print("to be implemented")

    return d


def linesearchDirectionacqs(J, gradJ, alpha, d, m, p1=1.5, p2=0.5):
    while True:
        m1 = m + alpha * d
        J1 = computeCostFunction(acqs, m1, client)
        succesArmijo = armijoCondition(J, J1, gradJ, alpha)
        if succesArmijo:
            succesGoldstein = goldsteinCondition(J, J1, gradJ, alpha)
            if succesGoldstein:
                return m1
            else:
                alpha*=p1
        else:
            alpha*=p2


def updateModel(acqs, m):
    print("To be implemented")



def print_pressure(pressure, ishot):
    print("\n" + "Pressure value at receivers for configuration " + str(ishot) + " : \n")
    print(pressure)
    print("\n")

def print_shot_config(shot_list, ishot):
    print("\n \n" + "Shot configuration number " + str(ishot) + " : \n")
    print(shot_list[ishot])

def print_group(group, indent=0):
    print("{}{}".format(" " * indent, group))

    indent += 4
    print("{}wrappers:".format(" " * indent))

    for wrapper in group.wrappers():
        print("{}{}".format(" " * (indent + 4), wrapper))
        print_with_indent(str(wrapper.value(False)), indent + 8)

    print("{}groups:".format(" " * indent))

    for subgroup in group.groups():
        print_group(subgroup, indent + 4)


def print_with_indent(msg, indent):
    indent_str = " " * indent
    print(indent_str + msg.replace("\n", "\n" + indent_str))


def print_flag(shot_list):
    i = 0
    for shot in shot_list:
        print("Shot " + str(i) + " status : " + shot.flag)
        i += 1
    print("\n")
