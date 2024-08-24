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
    TRUErms = [211.50,207.70,203.00,201.55,198.15,217,184.4]
    SensorRMS = [2.648809,2.646482,2.639234,2.635001,2.629366,2.652624,2.588314]

    # Grau do polinômio
    degree = 3

    # Calcula os coeficientes polinomiais
    coefficients = polynomial_regression(TRUErms, SensorRMS, degree)

    # Os coeficientes são retornados em ordem: [a_0, a_1, a_2, ..., a_n]
    print("Coeficientes:", coefficients)

    # Exemplo de envio dos coeficientes
    send = f"{coefficients[0]:.2f}|{coefficients[1]:.2f}|{coefficients[2]:.2f}"
    print(f"Enviando coeficientes: {send}")
    test_value = 2.609957

    value = (coefficients[3] 
                 + coefficients[2] * test_value 
                 + coefficients[1] * test_value**2 
                 + coefficients[0] * test_value**3
                 )
    print(value)
    

if __name__ == "__main__":
    main()

