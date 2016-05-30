# Global-Corruptor
This is a program that auto corrupts programs in such a way to yield glitchy results.

Currently, it runs a random address changer, where it takes a section of ram, picks two random bytes, one is a target which defines the value to be changed, and the value, which is what the target will be changed into when found.
This blanket approch causes a lot of courruption of ram, but is relitivly stable. Much more stable then the pecking technuique I used previously.
