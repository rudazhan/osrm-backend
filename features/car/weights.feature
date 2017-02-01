# by ruda
@routing @speed @traffic
Feature: Traffic - weights

    Background: grid with multiple intersections and traffic signal
        Given the node map
            """
              a
            b c d e
            h f g
            """
        And the nodes
          | node | id | highway         |
          | a    | 1  |                 |
          | b    | 2  |                 |
          | c    | 3  |                 |
          | d    | 4  | traffic_signals |
          | e    | 5  |                 |
          | f    | 6  |                 |
          | g    | 7  |                 |
          | h    | 8  |                 |

        And the ways
          | nodes | highway |
          | ac    | primary |
          | bc    | primary |
          | cd    | primary |
          | de    | primary |
          | cf    | primary |
          | cg    | primary |
          | ch    | primary |

        And the profile "car"

    Scenario: All weighting types
        When I route I should get
            | from | to | route           | time      |
            | b    | d  | bc,cd,cd        | 14s +-1   |
            | b    | a  | bc,ac,ac        | 19s +-1   |
            | b    | f  | bc,cf,cf        | 16s +-1   |
            | b    | g  | bc,cg,cg        | 17s +-1   |
            | b    | h  | bc,ch,ch        | 23s +-1   |
            | c    | e  | cd,de,de        | 16s +-1   |
