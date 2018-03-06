package karmaresearch.vlog;

import java.io.IOException;
import java.util.ArrayList;

/**
 * The <code>VLog</code> class exposes, at a low level, VLog to Java.
 */
public class VLog {

    static {
        System.loadLibrary("vlog_jni");
    }

    /**
     * Sets the log level of the VLog logger. Possible values of the parameter
     * are: "debug", "info", "warning", "error". The default log level is
     * "info". If the specified level string is not recognized, the default is
     * used.
     *
     * @param level
     *            the log level.
     *
     */
    public native void setLogLevel(String level);

    /**
     * Starts vlog with the specified edb configuration. The edb configuration
     * can either be specified directly as a string, in which case the
     * <code>isFile</code> parameter should be <code>false</code>, or as a file
     * name, in which case the <code>isFile</code> parameter should be
     * <code>true</code>.
     *
     * TODO: special exception for parse error in the configuration?
     *
     * @param edbconfig
     *            the edb configuration, as a string.
     * @param isFile
     *            whether it is a file, or an edb configuration as a string.
     * @exception IOException
     *                is thrown when the database could not be read for some
     *                reason.
     * @exception AlreadyStartedException
     *                is thrown when vlog was already started, and not stopped.
     */
    public native void start(String edbconfig, boolean isFile)
            throws AlreadyStartedException, IOException;

    /**
     * Starts vlog with the database as CSV files, in a directory specified by
     * the parameter. All files in this directory with suffix ".csv" are
     * considered. The predicate name is the prefix of the file name.
     *
     * @param directory
     *            the directory in which the CSV files live.
     * @exception IOException
     *                is thrown when the database could not be loaded for some
     *                reason.
     * @exception AlreadyStartedException
     *                is thrown when vlog was already started, and not stopped.
     */
    public native void startCSV(String directory)
            throws IOException, AlreadyStartedException;

    /**
     * Stops and de-allocates the reasoner. This method should be called before
     * beginning runs on another database. If vlog is not started yet, this call
     * does nothing, so it does no harm to call it more than once.
     */
    public native void stop();

    /**
     * Returns the internal representation of the predicate. Internally, VLog
     * uses integers to represent predicates. This method allows the user to
     * look up this internal number.
     *
     * @param predicate
     *            the predicate to look up
     * @return the predicate id, or -1 if not found.
     * @exception NotStartedException
     *                is thrown when vlog is not started yet.
     */
    public native int getPredicateId(String predicate)
            throws NotStartedException;

    /**
     * Returns the predicate. Internally, VLog uses integers to represent
     * predicates. This method allows the user to look up the predicate name,
     * when provided with the predicate id.
     *
     * @param predicateId
     *            the predicate to look up
     * @return the predicate string, or <code>null</code> if not found.
     * @exception NotStartedException
     *                is thrown when vlog is not started yet.
     */
    public native String getPredicate(int predicateId)
            throws NotStartedException;

    /**
     * Returns the internal representation of the constant. Internally, VLog
     * uses longs to represent constants. This method allows the user to look up
     * this internal number.
     *
     * @param constant
     *            the constant to look up
     * @return the constant id, or -1 if not found.
     * @exception NotStartedException
     *                is thrown when vlog is not started yet.
     */
    public native long getConstantId(String constant)
            throws NotStartedException;

    /**
     * Returns the constant. Internally, VLog uses longs to represent constants.
     * This method allows the user to look up the constant string, when provided
     * with the constant id.
     *
     * @param constantId
     *            the constant to look up
     * @return the constant string, or <code>null</code> if not found.
     * @exception NotStartedException
     *                is thrown when vlog is not started yet.
     */
    public native String getConstant(long constantId)
            throws NotStartedException;

    /**
     * Queries the current, so possibly materialized, database, and returns an
     * iterator that delivers the answers, one by one.
     *
     * TODO: is having variables as negative values OK?
     *
     * @param predicate
     *            the predicate id of the query
     * @param terms
     *            the constant values or variables. If the term is negative, it
     *            is assumed to be a variable.
     * @return the result iterator.
     * @exception NotStartedException
     *                is thrown when vlog is not started yet.
     */
    public native QueryResultEnumeration query(int predicate, long[] terms)
            throws NotStartedException;

    /**
     * Queries the current, so possibly materialized, database, and returns an
     * iterator that delivers the answers, one by one.
     *
     * TODO: is having variables start with a questionmark OK?
     *
     * TODO: deal with not-found predicates, terms.
     *
     * @param predicate
     *            the predicate of the query
     * @param terms
     *            the constants or variables. If the term starts with a question
     *            mark, it is a variable, otherwise it is a constant.
     * @return the result iterator.
     * @exception NotStartedException
     *                is thrown when vlog is not started yet.
     */
    public StringQueryResultEnumeration query(String predicate, String[] terms)
            throws NotStartedException {
        int intPred = getPredicateId(predicate);
        ArrayList<String> variables = new ArrayList<>();
        long[] longTerms = new long[terms.length];
        for (int i = 0; i < terms.length; i++) {
            if (terms[i].startsWith("?")) {
                boolean found = false;
                for (int j = 0; i < variables.size(); j++) {
                    if (variables.get(j).equals(terms[i])) {
                        found = true;
                        longTerms[i] = -j - 1;
                        break;
                    }
                }
                if (!found) {
                    variables.add(terms[i]);
                    longTerms[i] = -variables.size();
                }
            } else {
                longTerms[i] = getConstantId(terms[i]);
            }
        }
        return new StringQueryResultEnumeration(this,
                query(intPred, longTerms));
    }

    /**
     * Sets the rules for the VLog run. Any existing rules are removed.
     *
     * TODO: special exception for parse error in rules?
     *
     * TODO: make removal of existing rules a boolean parameter?
     *
     * @param rules
     *            the rules
     * @param rewriteMultipleHeads
     *            whether multiple-head rules should be rewritten when possible.
     * @exception NotStartedException
     *                is thrown when vlog is not started yet.
     */
    public native void setRules(String[] rules, boolean rewriteMultipleHeads)
            throws NotStartedException;

    /**
     * Sets the rules for the VLog run to the rules in the specified file. Any existing rules are removed.
     *
     * TODO: special exception for parse error in rules?
     * 
     * TODO: make removal of existing rules a boolean parameter?
     *
     * @param rulesFile
     *            the file name of the file containing the rules
     * @param rewriteMultipleHeads
     *            whether multiple-head rules should be rewritten when possible.
     *
     *
     * @exception NotStartedException
     *                is thrown when vlog is not started yet.
     * @exception IOException
     *                is thrown when the file could not be read for some reason
     */
    public native void setRulesFile(String rulesFile,
            boolean rewriteMultipleHeads)
            throws NotStartedException, IOException;

    /**
     * Materializes the database under the specified rules.
     *
     * TODO: maybe limit number of iterations? (Currently not in vlog, but could
     * be added)
     *
     * TODO: whether we should store the result of the materialization
     * somewhere, for instance as CSV files?
     *
     * @param skolem
     *            whether to use skolem chase <code>true</code> or restricted
     *            chase <code>false</code>.
     * @exception NotStartedException
     *                is thrown when vlog is not started yet.
     */
    public native void materialize(boolean skolem) throws NotStartedException;

    // For testing purposes ...
    public static void main(String[] args) throws Exception {
        VLog vlog = new VLog();
        vlog.start("blabla", false);
        vlog.stop();
        vlog.startCSV(
                "/Users/ceriel/Projects/chasebench/scenarios/doctors/data/10k");
        vlog.setRulesFile("/Users/ceriel/Projects/vlog-runs/data/doctors/rules",
                true);
        vlog.materialize(false);
        StringQueryResultEnumeration result = vlog.query("prescription",
                new String[] { "?ID", "?PATIENT", "186", "?C1" });
        while (result.hasMoreElements()) {
            String[] r = result.nextElement();
            for (String s : r) {
                System.out.print(s + " ");
            }
            System.out.println();
        }
        result.cleanup();
    }
}