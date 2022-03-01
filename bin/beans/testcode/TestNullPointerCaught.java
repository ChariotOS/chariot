public class TestNullPointerCaught {

	public static void main (String[] args) {
		int x[] = new int[10];

		try {
			x = null;
			System.out.println(x[1]);
		} catch (NullPointerException ex) {
			System.out.println("Caught null ptr\n");
		}

	}
}
