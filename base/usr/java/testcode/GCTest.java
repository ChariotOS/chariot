
import java.lang.Thread;

public class GCTest {

	public static void usemem () {
		int x[] = new int[1024];
	}

	public static void main (String[] args) 
		throws InterruptedException {
		int count = 100;
	
		while (count != 0) {
			usemem();
			Thread.sleep(100);
			count--;
		}
	} 
}
