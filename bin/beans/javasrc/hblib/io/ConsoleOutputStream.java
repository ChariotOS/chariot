package hblib.io;

import java.io.*;

/* Adopted from b2fj JVM implementation */

/**
 * This outputs chars to the screen.
 *
 * @author  Massimiliano Zattera
 * 	    Modifications by Kyle C. Hale 2017
 * @version 0.2
 */
public class ConsoleOutputStream extends PrintStream {

    public ConsoleOutputStream() {
		super(null);
	}

    /**
     * Writes a char using printf().
     * 
     * @param b
     */
    private native void putCharToStdout0 (int b);

    /**
     * Writes a String using printf().
     * 
     * @param s
     */
    private native void putStringToStdout0 (String s);
    
    /**
     * Writes a string followed by a newline character
     * to the underlying output stream.
     * 
     * @param s the string to print
     */
    private synchronized void println0(String s) {
        putStringToStdout0(s);
        putCharToStdout0('\n');
    }

    /**
     * Writes the specified byte to this output stream. The general
     * contract for <code>write</code> is that one byte is written
     * to the output stream. The byte to be written is the eight
     * low-order bits of the argument <code>b</code>. The 24
     * high-order bits of <code>b</code> are ignored.
     * <p>
     * Subclasses of <code>OutputStream</code> must provide an
     * implementation for this method.
     *
     * @param      b   the <code>byte</code>.
     * @exception  IOException  if an I/O error occurs. In particular,
     *             an <code>IOException</code> may be thrown if the
     *             output stream has been closed.
     */
    @Override
    public synchronized void write(int b) {
        putCharToStdout0(b);
    }
    
    /**
     * Writes a newline character
     * to the underlying output stream.
     */
    @Override
    public synchronized void println() {
   		putCharToStdout0('\n');
    }    
    
    /*** print() Delegates ***/
    
    @Override
    public synchronized void print(boolean v)
    {
    	putStringToStdout0(v ? "true" : "false");
    }
    
    @Override
    public synchronized void print(char v)
    {
   		putCharToStdout0(v);
    }
    
    @Override
    public synchronized void print(char[] v)
    {
    	putStringToStdout0(String.valueOf(v));
    }
    
    @Override
    public synchronized void print(double v)
    {
    	putStringToStdout0(String.valueOf(v));
    }
    
    @Override
    public synchronized void print(float v)
    {
    	putStringToStdout0(String.valueOf(v));
    }
    
    @Override
    public synchronized void print(int v)
    {
    	putStringToStdout0(String.valueOf(v));
    }
    
    @Override
    public synchronized void print(long v)
    {
    	putStringToStdout0(String.valueOf(v));
    }
    
    @Override
    public synchronized void print(Object v)
    {
    	putStringToStdout0(String.valueOf(v));
    }
    
    @Override
    public synchronized void print(String s) {
    	putStringToStdout0(s);
    }
    
    /*** println() Delegates ***/
    
    @Override
    public synchronized void println(boolean v)
    {
    	putStringToStdout0(v ? "true" : "false");
   		putCharToStdout0('\n');
    }
    
    @Override
    public synchronized void println(char v)
    {
   		putCharToStdout0(v);
   		putCharToStdout0('\n');
    }
    
    @Override
    public synchronized void println(char[] v)
    {
    	println0(String.valueOf(v));
    }
    
    @Override
    public synchronized void println(int v)
    {
    	println0(String.valueOf(v));
    }
    
    @Override
    public synchronized void println(long v)
    {
    	println0(String.valueOf(v));
    }
    
    @Override
    public synchronized void println(double v)
    {
    	println0(String.valueOf(v));
    }
    
    @Override
    public synchronized void println(float v)
    {
    	println0(String.valueOf(v));
    }
        
    @Override
    public synchronized void println(Object v)
    {
    	println0(String.valueOf(v));
    }
    
    @Override
    public synchronized void println(String s)
    {
    	println0(String.valueOf(s));
    }
}
