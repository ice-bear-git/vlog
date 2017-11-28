import argparse
import copy
import os
from subprocess import check_output, STDOUT, TimeoutExpired, CalledProcessError
from patsy import dmatrices
from sklearn.linear_model import LogisticRegression
from sklearn import metrics
import pandas as pd
import numpy as np

def parse_args():
    parser = argparse.ArgumentParser(description = "Ablation Study")
    parser.add_argument('--train_data', type=str, required=True, help='Training data csv file')
    parser.add_argument('--test_data', type=str, required=True, help='Test data csv file')
    return parser.parse_args()

COLUMNS = ["cost", "estimate", "countRules", "countUniqueRules", "countQueries", "algorithm"]

def perf_measures(yActual, yPredicted):
    TP = 0
    FP = 0
    FN = 0
    TN = 0

    if (len(yActual) != len(yPredicted)):
        print("FATAL", len(yActual) , " ! = " , len(yPredicted))
        return 1,1,1,1
    for i in range(len(yPredicted)):
        if yActual[i] == yPredicted[i]:
            TP += 1
            TN += 1
        else:
            FN += 1
            FP += 1
    return float(TP), float(FP), float(TN), float(FN)

def train_and_eval(train_file, test_file, i, n):
    FEATURES = copy.deepcopy(COLUMNS)
    if i >= 0:
        del(FEATURES[i])

    df_train = pd.read_csv(
      train_file,
      names=FEATURES,
      skipinitialspace=True,
      engine="python")
    df_test = pd.read_csv(
      test_file,
      names=FEATURES,
      skipinitialspace=True,
      engine="python")

    # remove NaN elements
    df_train = df_train.dropna(how='any', axis=0)
    df_test = df_test.dropna(how='any', axis=0)

    i = 0;
    dmatString = FEATURES[n-2] + ' ~'
    while i < n-2:
        dmatString += FEATURES[i]
        if i < n-3:
            dmatString += '+'
        i += 1

    #y,X = dmatrices ('algorithm ~ subjectBound + objectBound + numberOfResults + \
    #numberOfRules + numberOfQueries + numberOfUniqueRules', df_train, return_type = "dataframe")
    y, X = dmatrices(dmatString, df_train, return_type = "dataframe")
    y = np.ravel(y)
    model = LogisticRegression()
    model = model.fit(X, y)

    #yTest, xTest = dmatrices ('algorithm ~ subjectBound + objectBound + numberOfResults + \
    #numberOfRules + numberOfQueries + numberOfUniqueRules', df_test, return_type = "dataframe")
    yTest, xTest = dmatrices(dmatString, df_test, return_type = "dataframe")
    # check the accuracy on the training set

    predicted = model.predict(xTest)

    TP, FP, TN, FN = perf_measures(list(yTest.values), list(predicted))

    #print ("TP: ", TP)
    #print ("TN: ", TN)
    #print ("FP: ", FP)
    #print ("FN: ", FN)
    precision = TP / (TP + FP)
    recall = TP / (TP + FN)
    f1score = 2*precision * recall / (precision + recall)
    #print (metrics.accuracy_score(yTest, predicted))
    print ("Precision = ", precision)
    print("Recall = ", recall)
    print("Accuracy = ", f1score)
    return f1score

def normalizeColumn(fileName, column):
    #TODO: This file normalizes the said column from the file
    # and returns the new file
    xTemp = []
    with open(fileName, 'r') as fin:
        for line in fin.readlines():
            # split into columns and calculate mean, min, max
            columns = line.split(',')
            xTemp.append(float(columns[column]))
    X = np.array(xTemp, dtype=np.float64)
    avgX = np.mean(X)
    minX = np.min(X)
    maxX = np.max(X)

    for i,x in enumerate(X):
        X[i] = float(x - avgX) / float(maxX - minX)

    newRecords = ""
    with open(fileName, 'r') as fin:
        for row, line in enumerate(fin.readlines()):
            columns = line.split(',')
            cntColumns = len(columns)
            for i,col in enumerate(columns):
                if i == column:
                    newRecords += str(X[row])
                else:
                    newRecords += str(col)
                if i < cntColumns-1:
                    newRecords += ","

    newFileName = os.path.splitext(fileName)[0] + "-normalized.csv"
    with open(newFileName, 'w') as fout:
        fout.write(newRecords)
    return newFileName

def generate_feature_files(train, phase):
    with open(train, 'r') as fin:
        lines = fin.readlines()
        n = len(lines[0].split(','))
        data = [""] * (n-1)
        for line in lines:
            columns = line.split(',')
            i = 0
            while i < n-1:
                # Make a deep copy of columns
                features = copy.deepcopy(columns)
                # Delete the ith column
                del(features[i])
                data[i] += ",". join(features)
                i += 1
        print("Finished making features")
        i = 0
        while i < n-1:
            print ("Writing file for feature", str(i+1))
            with open('feature-'+ phase + str(i+1) + '.csv', 'w') as fout:
                fout.write(data[i])
            i += 1
    return n

args = parse_args()
train = args.train_data
test = args.test_data

train_normalized = normalizeColumn(train, 0)
test_normalized = normalizeColumn(test, 0)

n = generate_feature_files(train_normalized, "train")
generate_feature_files(test_normalized, "test")


accuracy = train_and_eval(train_normalized, test_normalized, -1, n+1)
print ("Overall accuracy = ", accuracy)

# Run linear model
i = 0
histogramData= ""
while i < n-1:
    train = 'feature-'+ 'train' + str(i+1) + '.csv'
    test = 'feature-' + 'test' + str(i+1) + '.csv'

    accuracy = train_and_eval(train, test, i, n)
    print ("#### without feature  (", i+1, ")" , COLUMNS[i], ": accuracy = ", accuracy )
    histogramData += COLUMNS[i] + " " + str(accuracy) + "\n"
    # Parse output and get the accuracy
    i += 1

with open('ablation-result.csv', 'w') as fout:
    fout.write("Feature Accuracy\n")
    fout.write(histogramData)

# Clean up
i = 0
while i < n-1:
    os.remove('feature-'+ 'train' + str(i+1) + '.csv')
    os.remove('feature-'+ 'test' + str(i+1) + '.csv')
    i += 1
os.remove(train_normalized)
os.remove(test_normalized)
