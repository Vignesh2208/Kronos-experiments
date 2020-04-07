import json
import os

books = {}

with open('books.json', 'r') as f:
    books = json.load(f)

pruned_books = {}

n_books = 100
for i in xrange(1, n_books + 1) :
    print "Book ID = ", str(i)
    print "Details = ", books[str(i)]
    pruned_books[str(i)] = 'books/%s_book_details.txt' %(str(i))
    os.system('mkdir -p books')
    os.system('touch books/%s_book_details.txt' %(str(i)))
    with open('books/%s_book_details.txt' %(str(i)), 'w') as f:
        f.write(str(books[str(i)]))
    

with open('pruned_books.json', 'w') as f:
    json.dump(pruned_books, f)
