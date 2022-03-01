public class TestNullPointer {

	public static void main (String[] args) {
		int x[] = new int[10];
		x = null;
		System.out.println(x[1]);
	}
}
