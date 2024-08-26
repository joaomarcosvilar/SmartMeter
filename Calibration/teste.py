import numpy as np
import matplotlib.pyplot as plt
import pandas as pd


def polynomial_regression(TRUErms, SensorRMS, degree=2):
    X = np.array(SensorRMS)
    print(X)
    Y = np.array(TRUErms)
    print(Y)
    coefficients = np.polyfit(X,Y,degree)
    
    return coefficients

def main():
    TRUErms = [7.540,5.415,5.410]
    SensorRMS = [2.560065,2.559481,2.558882]


    # Grau do polinômio
    degree = 1

    # Calcula os coeficientes polinomiais
    coefficients = polynomial_regression(TRUErms, SensorRMS, degree)

    # Os coeficientes são retornados em ordem: [a_0, a_1, a_2, ..., a_n]
    print("Coeficientes:", coefficients)

    # Exemplo de envio dos coeficientes
    send = f"{coefficients[0]:.2f}|{coefficients[1]:.2f}"
    print(f"Enviando coeficientes: {send}")
    test_value = SensorRMS[0]

    value = (coefficients[1] 
                 + coefficients[0] * test_value 
                #  + coefficients[0] * test_value**2 
                #  + coefficients[0] * test_value**3
                 )
    print(value)
    

if __name__ == "__main__":
    main()

