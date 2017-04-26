import os
import subprocess
import sys
import random
import argparse
from collections import defaultdict
from random import shuffle
from subprocess import check_output, STDOUT, TimeoutExpired


def generateQueries(rule, arity, resultRecords):

    queries = {}

    # Generic query that results in all the possible records
    # Example : ?RP0(A,B)
    query = rule + "("
    for i in range(arity):
        query += chr(i+65)
        if (i != arity-1):
            query += ","
    query += ")"
    queries[100+(len(resultRecords))] = [query]

    # Queries by replacing each variable by a constant
    # We use variable for i and constants for other columns from result records
    if arity > 1:
        for i, key in enumerate(sorted(resultRecords)):
            # i will be the type for us
            for record in resultRecords[key]:
                for a in range(arity):
                    query = rule + "("
                    for j, column in enumerate(record):
                        if (a == j):
                            query += chr(i+65)
                        else:
                            query += column

                        if (j != len(record) -1):
                            query += ","
                    query += ")"

                    # Fix the number of types
                    # If step count is >3, write 50 as the query type
                    if (i > 3):
                        if 50 in queries:
                            queries[50].append(query)
                        else:
                            queries[50] = [query]
                    else:
                        if (i+1 in queries):
                            queries[i+1].append(query)
                        else:
                            queries[i+1] = [query]

    # Boolean queries 
    for i, key in enumerate(sorted(resultRecords)):
        # i will be the type for us
        for record in resultRecords[key]:
            for a in range(arity):
                query = rule + "("
                for j, column in enumerate(record):
                    query += column
                    if (j != len(record) -1):
                        query += ","
                query += ")"
                if (1000+i+1 in queries):
                    queries[1000+i+1].append(query)
                else:
                    queries[1000+i+1] = [query]

    runQueries(queries)

'''
This function takses the queries dictionary has the input.
Query type is the key and list of queries of that type is the value.
'''
def runQueries(queries):
    for queryType in queries.keys():
        shuffle(queries[queryType])
        data = "Query Type QSQR MagicSets\n"
        iterations = 0
        for q in queries[queryType]:
            # Here invoke vlog and execute query and get the runtime
            try:
                outQSQR = check_output(['../vlog', 'queryLiteral' ,'-e','edb.conf', '--rules', 'dlog/LUBM1_LE.dlog', '--reasoningAlgo', 'qsqr', '-l', 'info', '-q', q], stderr=STDOUT, timeout=ARG_TIMEOUT)
            except TimeoutExpired:
                outQSQR = "Runtime = >" + str(ARG_TIMEOUT) + "K milliseconds"

            strQSQR = str(outQSQR)
            index = strQSQR.find("Runtime =")
            timeQSQR = strQSQR[index+10:strQSQR.find("milliseconds")-1]

            try:
                outMagic = check_output(['../vlog', 'queryLiteral' ,'-e','edb.conf', '--rules', 'dlog/LUBM1_LE.dlog', '--reasoningAlgo', 'magic', '-l', 'info', '-q', q], stderr=STDOUT, timeout=ARG_TIMEOUT)
            except TimeoutExpired:
                outMagic = "Runtime = >" + str(ARG_TIMEOUT) + "K milliseconds"

            strMagic = str(outMagic)
            index = strMagic.find("Runtime =")
            timeMagic = strMagic[index+10:strMagic.find("milliseconds")-1]

            record = str(q) + " " + str(queryType) + " " + str(timeQSQR) + " " + str(timeMagic)

            print (record)
            data += record + "\n"
            
            iterations += 1
            if iterations == ARG_NQ:
                break

    with open(outFile, 'a') as fout:
        fout.write(data)

def blocks(fileObject, size=65536):
    while True:
        b = fileObject.read(size)
        if not b:
            break
        yield b

def isFileTooBig(fileName):
    with open(fileName, "r") as fileObject:
        lineCount = sum(block.count("\n") for block in blocks(fileObject))

    if lineCount > ARG_BIGFILE:
        return True
    return False

def parseResultFile(name, resultFile):
    print (name)
    results = defaultdict(list)
    arity = 0
    with open(resultFile, 'r') as fin:
        lines = fin.readlines()
        # If file is too big, then randomly sample 10K records
        if isFileTooBig(resultFile):
            lines = random.sample(lines, ARG_SAMPLE)

    for line in lines:
        columns = line.split()
        arity = len(columns) - 1
        operands = []
        for i, column in enumerate(columns):
            if i == 0:
                continue
            operands.append(column)

        results[int(columns[0])].append(operands)

    generateQueries(name, arity, results)
    #print (len(results))

'''
Takes rule file and rule names array as the input
For every rule checks if we got any resul
'''
def parseRulesFile(rulesFile, rulesWithResult):
    exploredRules = set()
    with open(rulesFile, 'r') as fin:
        lines = fin.readlines()
        for line in lines:
            head = line.split(':')[0]
            body = line.split('-')[1]
            rule = head.split('(')[0]
            if rule in exploredRules:
                continue
            exploredRules.add(rule)
            if rule in rulesWithResult:
                #print (head, "=>", body)
                parseResultFile(rule, rulesWithResult[rule])

def parse_args():
    parser = argparse.ArgumentParser(description="Run Query generation")
    parser.add_argument('--rules' , type=str, help = 'Path to the rules file')
    parser.add_argument('--mat', type=str, help = 'Path to the materialized directory')
    parser.add_argument('--nq', type=int, help = "Number of queries to be executed of each type per predicate", default = 3)
    parser.add_argument('--timeout', type=int, help = "Number of seconds to wait for long running vlog process", default = 25)
    parser.add_argument('--sample', type=int, help = "Number of lines to sample from the big materialized files", default = 50000)
    parser.add_argument('--bigfile', type=int, help = "Number of lines file should contain so as to be categorized as a big file", default = 1000000)
    parser.add_argument('--out', type=str, help = 'Name of the query output file')

    return parser.parse_args()

args = parse_args()
ARG_TIMEOUT = args.timeout
ARG_SAMPLE = args.sample
ARG_BIGFILE = args.bigfile
ARG_NQ = args.nq
resultFiles = []
rulesFile = args.rules
outFile = args.out

with open(outFile, 'w') as fout:
    fout.write("")

rulesWithResult = dict()
matDir = args.mat
for root, dirs, files in os.walk(os.path.abspath(matDir)):
    for f in files:
        if not f.startswith('R'):
            continue
        ruleResultFilePath = os.path.join(root, f)
        rulesWithResult[f] = ruleResultFilePath
        resultFiles.append(ruleResultFilePath)
        #print (f)

parseRulesFile(rulesFile, rulesWithResult)

