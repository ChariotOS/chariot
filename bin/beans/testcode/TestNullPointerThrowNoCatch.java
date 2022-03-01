public class TestNullPointerThrowNoCatch {

	public static void doit () {
		throw new ArithmeticException("Bah ha ha");
	}

	public static void main (String[] args) {
		System.out.println("Trying to doit");
		doit();
	}
	
}
