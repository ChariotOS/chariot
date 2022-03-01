public class TestNullPointerThrow {

	public static void doit () {
		throw new ArithmeticException("Bah ha ha");
	}

	public static void main (String[] args) {
		try {
			System.out.println("Trying to doit");
			doit();
		} catch (ArithmeticException ex) {
			System.out.println("Caught my own ball");
		}
	}
	
}
